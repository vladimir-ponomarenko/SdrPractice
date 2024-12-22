import numpy as np
import matplotlib.pyplot as plt

# Открываем файл для чтения
with open('txdata.pcm', 'r') as file:
#with open('/build/tests/soapy_pluto/txdata.txt', 'r') as file:
    # Читаем все строки из файла
    lines = file.readlines()

# Инициализируем список для хранения данных
data = []
imag = []
real = []
count = []


# Обрабатываем каждую строку
counter  = 0
for line in lines:
    line = line.strip()  # Убираем лишние пробелы
    if line:  # Пропускаем пустые строки
        try:
            # Разделяем строку по запятой и преобразуем в числа
            numbers = list(map(float, [value.strip() for value in line.split(',')]))
            # Создаём комплексное число (I + jQ)
            complex_number = complex(numbers[0], numbers[1])
            data.append(complex_number)
            imag.append(numbers[1])
            real.append(numbers[0])
            count.append(counter)
            counter += 1
        except ValueError:
            print(f"Ошибка преобразования строки: {line}")

fig, axs = plt.subplots(2, 1, layout='constrained')
plt.figure(1)
axs[1].plot(count, np.abs(data),  color='grey')  # Используем scatter для диаграммы созвездия
axs[0].plot(count,(imag),color='red')  # Используем scatter для диаграммы созвездия
axs[0].plot(count,(real), color='blue')  # Используем scatter для диаграммы созвездия
plt.show()
