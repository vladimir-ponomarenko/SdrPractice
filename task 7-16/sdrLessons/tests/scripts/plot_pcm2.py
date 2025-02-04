import numpy as np
import matplotlib.pyplot as plt

# Чтение данных из PCM файла
def read_pcm_file(filename):
    real = []
    imag = []
    with open(filename, "rb") as f:
        index = 0
        while (byte := f.read(2)):
            if index % 2 == 0:
                real.append(int.from_bytes(byte, byteorder='little', signed=True))
            else:
                imag.append(int.from_bytes(byte, byteorder='little', signed=True))
            index += 1
    return np.array(real), np.array(imag)

# Определение детектора временной ошибки (TED)
def timing_error_detector(I, Q, offset, Nsp):
    e = (I[offset + Nsp] - I[offset]) * I[offset + Nsp // 2] + (Q[offset + Nsp] - Q[offset]) * Q[offset + Nsp // 2]
    return e

# Фильтр контура (пропорционально-интегрирующее звено)
def loop_filter(e, p1, p2, K1, K2):
    p1 = e * K1
    p2 += p1 + e * K2
    if p2 > 1:
        p2 -= 1
    return p1, p2

# Генератор стробирующих импульсов (NCC)
def ncc_control_signal(r, Nsp):
    return (r + 1) % Nsp

# Основной процесс символьной синхронизации
def symbol_sync(real, imag, Nsp=16, K1=0.05, K2=0.02):
    offset = 0
    p1 = p2 = 0
    Ns = len(real)
    timing_error_values = []

    for n in range(Ns - Nsp):
        e = timing_error_detector(real, imag, offset, Nsp)
        timing_error_values.append(e)
        
        p1, p2 = loop_filter(e, p1, p2, K1, K2)
        
        # Уточнить смещение
        offset = round(p2 * Nsp)
        if offset >= Nsp:
            offset = Nsp - 1
            p2 -= 1 

    return timing_error_values

filename = "/mnt/ssd1/sdr/sdrLessons/build/tests/soapy_pluto/txdata_bark4.pcm"

# Чтение данных
real, imag = read_pcm_file(filename)

# Символьная синхронизация
timing_errors = symbol_sync(real, imag)

# Построение графиков
plt.figure(figsize=(12, 12))

# График временной ошибки
plt.subplot(3, 1, 1)
plt.plot(timing_errors, color='blue', label='Timing Error')
plt.title('Timing Error Over Symbols')
plt.xlabel('Symbol Index')
plt.ylabel('Error')
plt.legend()

# График принимаемого сигнала
plt.subplot(3, 1, 2)
plt.plot(real, color='red', label='Real Part')
plt.plot(imag, color='green', label='Imaginary Part')
plt.title('Received Signal')
plt.xlabel('Sample Index')
plt.ylabel('Amplitude')
plt.legend()

# Диаграмма созвездия
plt.subplot(3, 1, 3)
plt.scatter(real, imag, color='purple', s=1)
plt.title('Constellation Diagram')
plt.xlabel('In-Phase (I)')
plt.ylabel('Quadrature (Q)')
plt.axis('equal')
plt.grid()

plt.tight_layout()
plt.show()