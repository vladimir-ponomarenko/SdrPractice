import numpy as np
import matplotlib.pyplot as plt

file_path = "/home/pluto_sdr/Загрузки/sdr/sdrLessons/build/tests/soapy_pluto/txdata_bark2.pcm"

# Списки для хранения данных
real = []
imag = []
count = []
counter = 0

with open(file_path, "rb") as f:
    index = 0
    while (byte := f.read(2)):
        if index % 2 == 0:
            real.append(int.from_bytes(byte, byteorder='little', signed=True))
            count.append(counter)
            counter += 1
        else:
            imag.append(int.from_bytes(byte, byteorder='little', signed=True))
        index += 1

real = np.array(real)
imag = np.array(imag)

symbol_duration = 10  # Длительность символа
Ns = 10  # Количество отсчетов на символ
num_symbols = min(len(real), len(imag)) // Ns  # количество символов в данных

plt.figure(figsize=(10, 6))

for i in range(num_symbols):
    start_index = i * Ns
    end_index = start_index + Ns
    
    plt.plot(real[start_index:end_index], imag[start_index:end_index], color='gray', alpha=0.5)

plt.xlabel('Реальная часть')
plt.ylabel('Мнимая часть')
plt.grid()
plt.xlim(-2000, 2000)
plt.ylim(-2000, 2000)
plt.axhline(0, color='black', lw=1)
plt.axvline(0, color='black', lw=1)
plt.show()

# TED
def timing_error_detector(real, imag, offset, Nsp):
    e = (real[offset + Nsp] - real[offset]) * (imag[offset + Nsp // 2] - imag[offset])
    return e

offset = 0
timing_error = timing_error_detector(real, imag, offset, Ns)
print(f"Timing Error: {timing_error}")

# NCC
class NCC:
    def __init__(self, Ns):
        self.Ns = Ns
        self.count = 0

    def generate_pulse(self):
        self.count += 1
        if self.count >= self.Ns:
            self.count = 0
            return True
        return False

# NCC для выборки отсчетов
ncc = NCC(Ns)

for i in range(len(real)):
    if ncc.generate_pulse():
        print(f"Taking sample at index: {i}")
