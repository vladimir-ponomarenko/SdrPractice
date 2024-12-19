import numpy as np
import matplotlib.pyplot as plt


with open('sdrLessons/src/rx_data.txt', 'r') as file:
    data = file.readlines()


complex_numbers = []
for line in data:
    x, y = map(float, line.strip().split(','))
    complex_numbers.append(complex(x, y))


real_parts = [z.real for z in complex_numbers]
imaginary_parts = [z.imag for z in complex_numbers]


plt.figure(figsize=(10, 10))
plt.subplot(2,1,1)
plt.scatter(real_parts, imaginary_parts, color='blue', marker='o')


plt.title('График комплексных чисел')
plt.xlabel('Действительная часть')
plt.ylabel('Мнимая часть')
plt.axhline(0, color='black',linewidth=0.5, ls='--')
plt.axvline(0, color='black',linewidth=0.5, ls='--')
plt.grid()
# plt.xlim(-1000, 1000)
# plt.ylim(-1000, 10000)
plt.subplot(2,1,2)
plt.plot(real_parts)
plt.plot(imaginary_parts)


plt.show()