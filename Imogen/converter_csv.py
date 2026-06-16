#!/usr/bin/env python3
import os
import csv

def mv_to_ppm(mv):
    if mv <= 0:
        return 0.0
    if mv >= 5000:
        mv = 4999
    v_rl = mv / 1000.0
    r_l = 10.0  # kOhm
    v_c = 5.0   # Volts
    r_s = r_l * (v_c - v_rl) / v_rl
    r_0 = 1.48  # kOhm (calibrado para ar limpo: ~1000mV resulta em Rs=40k, Rs/R0=27, ~0.65 ppm)
    ratio = r_s / r_0
    ppm = 100.0 * (ratio ** -1.53)
    return round(ppm, 3)

def convert_csv(file_path):
    if not os.path.exists(file_path):
        print(f"Arquivo não encontrado: {file_path}")
        return

    print(f"Convertendo {file_path}...")
    temp_file = file_path + ".tmp"
    
    with open(file_path, mode='r', encoding='utf-8') as infile, \
         open(temp_file, mode='w', newline='', encoding='utf-8') as outfile:
        
        reader = csv.reader(infile)
        writer = csv.writer(outfile)
        
        header = next(reader, None)
        if not header:
            print(f"Arquivo vazio: {file_path}")
            return
            
        # Determinar índices das colunas
        try:
            mv_idx = header.index("CO_Voltagem_mV")
        except ValueError:
            print("Coluna 'CO_Voltagem_mV' não encontrada no cabeçalho!")
            return
            
        pct_idx = None
        if "CO_Percentual" in header:
            pct_idx = header.index("CO_Percentual")
            header[pct_idx] = "CO_PPM"
        elif "CO_PPM" not in header:
            header.append("CO_PPM")
            
        writer.writerow(header)
        
        for row in reader:
            if not row:
                continue
            
            # Tratamento de linhas de erro
            if len(row) <= mv_idx or row[mv_idx] == "" or row[mv_idx] == "ERRO":
                # Preserva a linha de erro original
                if pct_idx is not None and pct_idx < len(row):
                    row[pct_idx] = ""
                writer.writerow(row)
                continue
                
            try:
                mv_val = int(row[mv_idx])
                ppm_val = mv_to_ppm(mv_val)
            except ValueError:
                # Caso o valor não seja conversível para inteiro, grava como está
                writer.writerow(row)
                continue
                
            if pct_idx is not None:
                row[pct_idx] = ppm_val
            else:
                row.append(ppm_val)
                
            writer.writerow(row)
            
    # Substitui o arquivo original pelo temporário
    os.replace(temp_file, file_path)
    print(f"Sucesso ao converter {file_path}!")

if __name__ == "__main__":
    paths = [
        "dados_sensores.csv",
        os.path.join("Imogen", "Imogen", "dados_sensores.csv"),
        "c:/Users/Operador/Documents/Entrega_Final/dados_sensores.csv",
        "c:/Users/Operador/Documents/Entrega_Final/Imogen/Imogen/dados_sensores.csv"
    ]
    
    unique_paths = list(set(os.path.abspath(p) for p in paths))
    for p in unique_paths:
        if os.path.exists(p):
            convert_csv(p)
