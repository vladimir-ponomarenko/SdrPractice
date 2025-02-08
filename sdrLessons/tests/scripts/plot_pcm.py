#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import numpy as np
import matplotlib.pyplot as plt
from scipy.signal import convolve, firwin

def read_iq_data(filename: str, start_sample: int, end_sample: int) -> np.ndarray:
    """Читает I/Q данные из бинарного файла (формат int16, чередующиеся I/Q).

    Args:
        filename: Путь к бинарному файлу.
        start_sample: Начальный индекс сэмпла (значения int16, *не* пары I/Q).
        end_sample: Конечный индекс сэмпла (значения int16, *не* пары I/Q).

    Returns:
        Комплексный массив NumPy, представляющий I/Q данные.

    Raises:
        FileNotFoundError: Если файл не найден.
        ValueError: Если start_sample или end_sample некорректны.
    """
    if not isinstance(filename, str):
        raise TypeError("Имя файла должно быть строкой.")
    if not isinstance(start_sample, int) or not isinstance(end_sample, int):
        raise TypeError("start_sample и end_sample должны быть целыми числами.")
    if start_sample < 0 or end_sample <= start_sample:
        raise ValueError("Некорректные значения start_sample или end_sample (должно быть: 0 <= start_sample < end_sample).")

    try:
        with open(filename, "rb") as file:
            file.seek(start_sample * 2)  # int16 (это 2 байта)
            num_shorts_to_read = end_sample - start_sample
            raw_bytes = file.read(num_shorts_to_read * 2)
    except FileNotFoundError:
        raise FileNotFoundError(f"Файл не найден: {filename}") from None

    data_int16 = np.frombuffer(raw_bytes, dtype=np.int16)

    data_int16 = data_int16[:(len(data_int16) // 2) * 2]

    i_channel = data_int16[::2].astype(np.float64)
    q_channel = data_int16[1::2].astype(np.float64)
    return i_channel + 1j * q_channel


def gardner_timing_recovery(signal: np.ndarray, sps: int, algorithm: int = 3) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """Выполняет синхронизацию по времени, используя алгоритм Гарднера (или Мюллера и Мюллера).

    Args:
        signal: Входной комплексный сигнал (с избыточной дискретизацией).
        sps: Количество отсчётов на символ.
        algorithm: Выбор алгоритма: 3 (Gardner), 4 (Gardner-freq. offset), 5 (M&M).

    Returns:
        Кортеж: (синхронизированные_символы, ошибки_синхронизации, дробные_смещения_времени)

    Raises:
        ValueError: Если выбран недопустимый алгоритм или sps.
    """

    if not isinstance(signal, np.ndarray):
        raise TypeError("Входной сигнал должен быть массивом NumPy.")
    if not isinstance(sps, int) or sps < 1:
        raise ValueError("Количество отсчётов на символ (sps) должно быть целым числом >= 1.")
    if algorithm not in (3, 4, 5):
        raise ValueError("Недопустимый выбор алгоритма. Должно быть 3, 4 или 5.")
    if sps <= 2 and algorithm in (3, 4):
        raise ValueError("Алгоритмы Гарднера (3 и 4) требуют избыточной дискретизации (sps > 2).")


    # Параметры фильтра
    damping_factor = np.sqrt(2) / 2
    normalized_bandwidth = 0.01  # Нормированная полоса пропускания петли (BnTs)
    kp = 2.7  # Коэффициент усиления
    theta = (normalized_bandwidth) / (damping_factor + 0.25 / damping_factor)
    k1 = -4 * damping_factor * theta / ((1 + 2 * damping_factor * theta + theta**2) * kp)
    k2 = -4 * theta**2 / ((1 + 2 * damping_factor * theta + theta**2) * kp)


    synced_symbols = []
    errors = []
    offsets = []

    p1 = 0.0
    p2 = 0.0
    sample_index = 0

    while True:
        interpolated_index = int(sample_index + round(p2 * sps))

        if interpolated_index + sps + sps // 2 >= len(signal):
            break

        current_sample = signal[interpolated_index]
        next_symbol_sample = signal[interpolated_index + sps]
        midpoint_sample = signal[interpolated_index + sps // 2]

        # Детектор ошибки синхронизации (TED)
        if algorithm == 3:  # Стандартный Гарднер
            error = -((next_symbol_sample.real - current_sample.real) * midpoint_sample.real +
                      (next_symbol_sample.imag - current_sample.imag) * midpoint_sample.imag)
        elif algorithm == 4:  # Гарднер (устойчивый к сдвигу частоты)
            error = -np.real((np.conj(next_symbol_sample) - np.conj(current_sample)) * midpoint_sample)
        elif algorithm == 5:  # Мюллер и Мюллер
            error = -(current_sample.real * np.sign(next_symbol_sample.real) -
                      next_symbol_sample.real * np.sign(current_sample.real) +
                      current_sample.imag * np.sign(next_symbol_sample.imag) -
                      next_symbol_sample.imag * np.sign(current_sample.imag))
        else:
            raise ValueError("Недопустимый алгоритм (не должно произойти).")

        synced_symbols.append(current_sample)
        errors.append(error)

        p1_update = k1 * error 
        p2 += (p1_update + k2 * error)

        p2 = np.mod(p2, 1.0)

        offsets.append(p2 * sps)
        sample_index += sps

    return np.array(synced_symbols), np.array(errors), np.array(offsets)


def plot_eye_diagram(signal: np.ndarray, sps: int, num_traces: int = 200, title: str = "Глазковая диаграмма", ax: plt.Axes = None):
    """Генерирует глазковую диаграмму из вещественной части сигнала.

    Args:
        signal: Входной сигнал (комплексный или вещественный).
        sps: Количество отсчётов на символ.
        num_traces: Количество трасс для наложения.
        title: Заголовок графика.
        ax: Объект Matplotlib Axes (необязательно). Если None, создается новая фигура.
    """
    if not isinstance(signal, np.ndarray):
        raise TypeError("Сигнал должен быть массивом NumPy.")
    if not isinstance(sps, int) or sps <= 0:
         raise ValueError("Количество отсчётов на символ (sps) должно быть положительным целым числом.")
    if not isinstance(num_traces, int) or num_traces <=0:
        raise ValueError("Количество должно быть положительным.")

    real_signal = np.real(signal)
    max_symbols = len(real_signal) // sps
    num_traces = min(num_traces, max_symbols)

    if ax is None:
        fig, ax = plt.subplots()

    for i in range(num_traces):
        start_index = i * sps
        if start_index + 2 * sps > len(real_signal):
            break
        segment = real_signal[start_index : start_index + 2 * sps]
        time_axis = np.arange(len(segment))
        ax.plot(time_axis, segment, color='blue', alpha=0.1)

    ax.set_title(title)
    ax.set_xlabel("Отсчёты")
    ax.set_ylabel("Амплитуда")
    ax.grid(True)


def plot_constellation(signal: np.ndarray, title: str = "Диаграмма созвездия", ax: plt.Axes = None):
    """Создает диаграмму созвездия (график рассеяния I/Q).

    Args:
        signal: Входной комплексный сигнал.
        title: Заголовок графика.
        ax: Объект Matplotlib Axes (необязательно).
    """

    if not isinstance(signal, np.ndarray):
        raise TypeError("Сигнал должен быть массивом NumPy.")
    if np.iscomplexobj(signal) is False:
        raise ValueError("Сигнал должен быть комплексным массивом NumPy.")

    if ax is None:
        fig, ax = plt.subplots()

    ax.scatter(signal.real, signal.imag, color='blue', s=5, alpha=0.6)
    ax.axhline(0, color='black', linestyle='--', linewidth=0.8)
    ax.axvline(0, color='black', linestyle='--', linewidth=0.8)
    ax.set_title(title)
    ax.set_xlabel("Синфазная составляющая (I)")
    ax.set_ylabel("Квадратурная составляющая (Q)")
    ax.grid(True)
    ax.set_aspect('equal', 'box')


def create_raised_cosine_filter(sps: int, filter_length: int, rolloff: float = 0.2) -> np.ndarray:
    """Создает фильтр RC.

    Args:
        sps: Количество отсчётов на символ.
        filter_length: Длина фильтра (в отсчётах).
        rolloff: Коэффициент сглаживания (от 0 до 1).

    Returns:
        Коэффициенты RC-фильтра.
    """
    if not all(isinstance(arg, int) for arg in [sps, filter_length]):
        raise TypeError("sps и filter_length должны быть целыми числами")
    if not 0 <= rolloff <= 1:
        raise ValueError("Коэффициент сглаживания должен быть между 0 и 1.")
    if filter_length % 2 == 0:
      raise ValueError("filter_length должно быть нечётным числом.")


    return firwin(filter_length, cutoff=1 / sps, window=('tukey', rolloff), pass_zero=True, scale=True)


def main():
    # --- Конфигурация ---
    config = {
        "filename": "/mnt/ssd1/sdr/sdrLessons/build/tests/soapy_pluto/data2.bin",
        "start_sample": 440 * 2,  # отсчёты int16
        "end_sample": 1330 * 2,   # отсчёты int16
        "sps": 10,
        "algorithm": 3,  # 3: Gardner, 4: Gardner (уст. к сдвигу), 5: M&M
        "filter_length": 11, # Должна быть нечётной для firwin с pass_zero=True
        "rolloff": 0.2, # Коэффициент сглаживания для RC фильтра.
    }

    try:
        iq_data = read_iq_data(config["filename"], config["start_sample"], config["end_sample"])
    except (FileNotFoundError, ValueError) as e:
        print(f"Ошибка загрузки данных: {e}")
        return

    iq_data = iq_data / (np.max(np.abs(iq_data)) + 1e-9)

    # --- Согласованная фильтрация ---
    matched_filter = create_raised_cosine_filter(config["sps"], config["filter_length"], config["rolloff"])
    filtered_iq_data = convolve(iq_data, matched_filter, mode='full')[:len(iq_data)]

    # --- Выделение символов (до и после синхронизации) ---
    raw_symbols = iq_data[::config["sps"]]
    filtered_symbols_pre_sync = filtered_iq_data[::config["sps"]]
    synced_symbols, errors, offsets = gardner_timing_recovery(filtered_iq_data, config["sps"], config["algorithm"])

    fig, axes = plt.subplots(3, 3, figsize=(18, 12))

    # Ряд 1: Сигналы во временной области
    axes[0, 0].plot(iq_data.real, label='I', color='blue')
    axes[0, 0].plot(iq_data.imag, label='Q', color='red')
    axes[0, 0].set_title("Необработанные I/Q данные")
    axes[0, 0].set_xlabel("Индекс отсчёта")
    axes[0, 0].set_ylabel("Амплитуда")
    axes[0, 0].grid(True)
    axes[0, 0].legend()

    axes[0, 1].plot(filtered_iq_data.real, label='I (Отфильтрованный)', color='blue')
    axes[0, 1].plot(filtered_iq_data.imag, label='Q (Отфильтрованный)', color='red')
    axes[0, 1].set_title("Отфильтрованные I/Q данные")
    axes[0, 1].set_xlabel("Индекс отсчёта")
    axes[0, 1].set_ylabel("Амплитуда")
    axes[0, 1].grid(True)
    axes[0, 1].legend()

    axes[0, 2].plot(errors, color='green')
    axes[0, 2].set_title("Ошибки синхронизации")
    axes[0, 2].set_xlabel("Индекс символа")
    axes[0, 2].set_ylabel("Ошибка")
    axes[0, 2].grid(True)

    # Ряд 2: Диаграммы созвездия
    plot_constellation(raw_symbols, "Необработанное созвездие (до фильтрации)", axes[1, 0])
    plot_constellation(filtered_symbols_pre_sync, "Отфильтрованное созвездие (до синхр.)", axes[1, 1])
    plot_constellation(synced_symbols, "Синхронизированное созвездие", axes[1, 2])

    # Ряд 3: Глазковые диаграммы и смещения
    plot_eye_diagram(filtered_iq_data, config["sps"], title="Глазковая диаграмма (отфильтрованный сигнал)", ax=axes[2, 0])
    plot_eye_diagram(synced_symbols, 1, title="Глазковая диаграмма (синхронизированный сигнал)", ax=axes[2, 1])  # sps=1

    axes[2, 2].plot(offsets, color='purple')
    axes[2, 2].set_title("Смещения синхронизации")
    axes[2, 2].set_xlabel("Индекс символа")
    axes[2, 2].set_ylabel("Смещение (отсчёты)")
    axes[2, 2].grid(True)

    plt.tight_layout()
    plt.show()


if __name__ == "__main__":
    main()