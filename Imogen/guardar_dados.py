#!/usr/bin/env python3
import sys
import os
import csv
import re
import time

import serial
import serial.tools.list_ports

CSV_FILE = "dados_sensores.csv"
BAUD_RATE = 115200

def mv_to_ppm(mv):
    if mv <= 0:
        return 0.0
    if mv >= 5000:
        mv = 4999
    v_rl = mv / 1000.0
    r_l = 10.0  # kOhm
    v_c = 5.0   # Volts
    r_s = r_l * (v_c - v_rl) / v_rl
    r_0 = 1.48  # kOhm
    ratio = r_s / r_0
    ppm = 100.0 * (ratio ** -1.53)
    return round(ppm, 3)

# Regex pattern matching the output of the STM32:
# "Temp:25.5C CO:1500mV(15.20ppm)" or "Temp:-2.3C CO:320mV(9%)"
DATA_PATTERN = re.compile(r"Temp:(-?\d+(?:\.\d+)?)C\s+CO:(\d+)mV\(([\d\.]+)(%|ppm)\)")

def select_serial_port():
    ports = serial.tools.list_ports.comports()
    if not ports:
        print("Nenhuma porta serial encontrada!")
        print("Verifique se a placa STM32 está conectada ao computador via USB.")
        sys.exit(1)
    
    # Filter for ports that might be the STM32 board (contains STMicroelectronics, STM, STLink, ACM, etc.)
    stm_ports = []
    for p in ports:
        desc = p.description or ""
        hwid = p.hwid or ""
        name = p.device or ""
        if "STMicroelectronics" in desc or "STM32" in desc or "ST-Link" in desc or "ACM" in name or "USB" in name:
            stm_ports.append(p)
            
    if len(stm_ports) == 1:
        print(f"Detectada placa na porta: {stm_ports[0].device} ({stm_ports[0].description})")
        return stm_ports[0].device
        
    print("Portas seriais disponíveis:")
    for idx, p in enumerate(ports):
        print(f"[{idx}] {p.device} - {p.description}")
        
    while True:
        try:
            choice = input(f"Selecione o número da porta (0-{len(ports)-1}): ")
            choice_idx = int(choice)
            if 0 <= choice_idx < len(ports):
                return ports[choice_idx].device
            else:
                print("Opção inválida.")
        except ValueError:
            print("Entrada inválida. Digite um número.")
        except KeyboardInterrupt:
            print("\nOperação cancelada.")
            sys.exit(0)

def main():
    port = select_serial_port()
    print(f"Conectando a {port} a {BAUD_RATE} bps...")
    
    try:
        ser = serial.Serial(port, BAUD_RATE, timeout=1)
    except Exception as e:
        print(f"Erro ao abrir a porta serial: {e}")
        print("Dica: Você pode precisar de permissão de acesso. Tente adicionar seu usuário ao grupo dialout:")
        print("  sudo usermod -aG dialout $USER")
        print("(Lembre-se de reiniciar sua sessão/fazer logout e login novamente para aplicar).")
        sys.exit(1)
        
    # Create/initialize CSV file with headers if it doesn't exist
    file_exists = os.path.exists(CSV_FILE)
    try:
        with open(CSV_FILE, mode='a', newline='', encoding='utf-8') as f:
            writer = csv.writer(f)
            if not file_exists:
                writer.writerow(["Timestamp", "Temperatura_C", "CO_Voltagem_mV", "CO_PPM"])
                print(f"Arquivo CSV '{CSV_FILE}' criado com os cabeçalhos.")
    except IOError as e:
        print(f"Erro ao inicializar o arquivo CSV: {e}")
        ser.close()
        sys.exit(1)
        
    print("\nLendo dados... Pressione Ctrl+C para encerrar.\n")
    print(f"{'Data e Hora':<20} | {'Temp (°C)':<10} | {'CO (mV)':<8} | {'CO (PPM)':<8}")
    print("-" * 60)
    
    try:
        # Flush input buffer to clear old data
        ser.reset_input_buffer()
        
        while True:
            if ser.in_waiting > 0:
                line_bytes = ser.readline()
                try:
                    line = line_bytes.decode('utf-8', errors='ignore').strip()
                except Exception:
                    continue
                
                if not line:
                    continue
                
                # Check for errors from LM75
                if "ERRO" in line or "LM75 nao responde" in line:
                    print(f"\033[91m[ERRO PLACA] {line}\033[0m")
                    # Log the error in the CSV as well
                    timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
                    try:
                        with open(CSV_FILE, mode='a', newline='', encoding='utf-8') as f:
                            writer = csv.writer(f)
                            writer.writerow([timestamp, "ERRO", "", ""])
                    except IOError as e:
                        print(f"Erro ao gravar erro no CSV: {e}")
                    continue
                
                match = DATA_PATTERN.search(line)
                if match:
                    temp_val = float(match.group(1))
                    co_mv = int(match.group(2))
                    val_str = match.group(3)
                    unit = match.group(4)
                    
                    # Se receber em %, converte para PPM. Se for ppm, usa o valor diretamente.
                    if unit == "%":
                        co_ppm = mv_to_ppm(co_mv)
                    else:
                        co_ppm = float(val_str)
                    
                    timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
                    
                    # Output to console nicely
                    print(f"{timestamp:<20} | {temp_val:<10.1f} | {co_mv:<8} | {co_ppm:<8.3f}")
                    
                    # Save to CSV
                    try:
                        with open(CSV_FILE, mode='a', newline='', encoding='utf-8') as f:
                            writer = csv.writer(f)
                            writer.writerow([timestamp, temp_val, co_mv, co_ppm])
                    except IOError as e:
                        print(f"\nErro ao escrever no CSV: {e}")
                else:
                    # Print raw line if it doesn't match standard format but is not an error
                    if line.strip():
                        print(f"[RAW] {line}")
                        
            time.sleep(0.01)
            
    except KeyboardInterrupt:
        print("\n\nLeitura encerrada pelo usuário.")
    except Exception as e:
        print(f"\nOcorreu um erro durante a execução: {e}")
    finally:
        if ser.is_open:
            ser.close()
            print("Porta serial fechada.")

if __name__ == "__main__":
    main()
