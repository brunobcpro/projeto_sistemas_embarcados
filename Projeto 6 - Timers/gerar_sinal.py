import serial
import matplotlib
matplotlib.use('Qt5Agg')
import matplotlib.pyplot as plt
from collections import deque

ser = serial.Serial('COM4', 115200, timeout=1)

dados = deque([0] * 10, maxlen=10)

plt.ion()
fig, ax = plt.subplots()
linha, = ax.plot(range(10), dados)
ax.set_xlim(0, 10)
ax.set_ylim(0, 255)
ax.set_title("Sinal DAC - PCF8591")
ax.set_xlabel("Amostras")
ax.set_ylabel("Valor (0-255)")

while True:
    try:
        linha_lida = ser.readline()
        print(f"DEBUG - Dado bruto lido da serial: {linha_lida}")
        valor = linha_lida.decode('utf-8', errors='ignore').strip()
        if valor:
            dados.append(int(valor))
            linha.set_ydata(dados)
            ax.relim()
            ax.autoscale_view()
            plt.draw()
            plt.pause(0.01)
    except ValueError:
        pass
    except serial.SerialException as e:
        print(f"\nErro na porta serial: {e}")
        break
    except KeyboardInterrupt:
        print("Encerrando...")
        ser.close()
        break