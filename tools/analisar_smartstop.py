import re
import csv
import os
import glob
import pandas as pd
from textwrap import dedent

# -------------------------------------------------------
# CONFIGURAÇÕES
# -------------------------------------------------------
BASE_DIR = r"C:\Users\joacy\Desktop\smartstop_bitdoglab"

CONSOLIDATED_OUTPUT = os.path.join(
    BASE_DIR, "smartstop_consolidated_summary.txt"
)

# -------------------------------------------------------
# EXPRESSÕES REGULARES PARA EXTRAIR DADOS
# -------------------------------------------------------

regex_header = re.compile(
    r"Ciclo:\s+(\d+)\s+\|\s+Andar:\s+(\d+)\s+\|\s+Dir:\s+(\w+)\s+\|\s+Ocupação:\s+(\d+)/(\d+)"
)
regex_event_decision = re.compile(r"DECISÃO:\s+Parar no andar\s+(\d+)")
regex_event_embarque = re.compile(r"EMBARQUE")
regex_event_desembarque = re.compile(r"DESEMBARQUE:\s+(\d+)")
regex_event_emergencia = re.compile(r"EMERGÊNCIA")
regex_event_skipped = re.compile(r"ignorando chamada")


# -------------------------------------------------------
# PARSE DO ARQUIVO DE LOG
# -------------------------------------------------------

def parse_log(log_path: str):
    if not os.path.exists(log_path):
        print(f"ERRO: Arquivo de log não encontrado: {log_path}")
        return []

    with open(log_path, "r", encoding="utf-8", errors="ignore") as f:
        linhas = f.readlines()

    dados = []

    entrada_atual = {
        "cycle": None,
        "floor": None,
        "direction": None,
        "occupancy": None,
        "capacity": None,
        "decision_floor": None,
        "embarked": 0,
        "disembarked": 0,
        "emergency": 0,
        "skipped_calls": 0,
    }

    for linha in linhas:
        # Cabeçalho principal de ciclo
        header = regex_header.search(linha)
        if header:
            # quando começa um novo ciclo, salva o anterior
            if entrada_atual["cycle"] is not None:
                dados.append(entrada_atual.copy())

            entrada_atual = {
                "cycle": int(header.group(1)),
                "floor": int(header.group(2)),
                "direction": header.group(3),
                "occupancy": int(header.group(4)),
                "capacity": int(header.group(5)),
                "decision_floor": None,
                "embarked": 0,
                "disembarked": 0,
                "emergency": 0,
                "skipped_calls": 0,
            }
            continue

        # Decisão de parada
        dec = regex_event_decision.search(linha)
        if dec:
            entrada_atual["decision_floor"] = int(dec.group(1))

        # Embarque detectado
        if regex_event_embarque.search(linha):
            entrada_atual["embarked"] += 1

        # Desembarque detectado
        des = regex_event_desembarque.search(linha)
        if des:
            entrada_atual["disembarked"] += int(des.group(1))

        # Emergência detectada
        if regex_event_emergencia.search(linha):
            entrada_atual["emergency"] = 1

        # Chamada ignorada (skipped stop)
        if regex_event_skipped.search(linha):
            entrada_atual["skipped_calls"] += 1

    # Salva o último ciclo
    if entrada_atual["cycle"] is not None:
        dados.append(entrada_atual.copy())

    return dados


# -------------------------------------------------------
# GERAÇÃO DE CSV / XLSX
# -------------------------------------------------------

def save_tables(dados, csv_path: str, xlsx_path: str):
    if not dados:
        return None

    cols = [
        "cycle", "floor", "direction", "occupancy", "capacity",
        "decision_floor", "embarked", "disembarked",
        "emergency", "skipped_calls"
    ]

    # CSV
    with open(csv_path, "w", newline="", encoding="utf-8") as csvfile:
        writer = csv.DictWriter(csvfile, fieldnames=cols)
        writer.writeheader()
        writer.writerows(dados)

    print(f"✔ CSV:   {csv_path}")

    # Excel
    df = pd.read_csv(csv_path)
    df.to_excel(xlsx_path, index=False)
    print(f"✔ XLSX:  {xlsx_path}")

    return df


# -------------------------------------------------------
# CÁLCULO DE MÉTRICAS DE QUALIDADE
# -------------------------------------------------------

def calcular_metricas(df: pd.DataFrame):
    total_ciclos = len(df)

    # Ocupação
    df["occupancy_ratio"] = df["occupancy"] / df["capacity"]
    ocupacao_media = df["occupancy_ratio"].mean() * 100
    ocupacao_max = df["occupancy_ratio"].max() * 100
    ciclos_acima_80 = (df["occupancy_ratio"] >= 0.8).sum()
    perc_acima_80 = ciclos_acima_80 / total_ciclos * 100 if total_ciclos > 0 else 0.0

    # Emergências
    total_emergencias = df["emergency"].sum()
    perc_emergencias = total_emergencias / total_ciclos * 100 if total_ciclos > 0 else 0.0

    # Chamadas ignoradas
    ciclos_com_skip = (df["skipped_calls"] > 0).sum()
    total_skips = df["skipped_calls"].sum()
    perc_ciclos_com_skip = ciclos_com_skip / total_ciclos * 100 if total_ciclos > 0 else 0.0

    # Fluxo de passageiros
    total_embarked = df["embarked"].sum()
    total_disembarked = df["disembarked"].sum()

    # Andares mais movimentados (parada com decisão)
    paradas_validas = df.dropna(subset=["decision_floor"])
    if not paradas_validas.empty:
        top_floors = (
            paradas_validas["decision_floor"]
            .value_counts()
            .head(3)
            .to_dict()
        )
    else:
        top_floors = {}

    metricas = {
        "total_ciclos": total_ciclos,
        "ocupacao_media": ocupacao_media,
        "ocupacao_max": ocupacao_max,
        "ciclos_acima_80": ciclos_acima_80,
        "perc_acima_80": perc_acima_80,
        "total_emergencias": total_emergencias,
        "perc_emergencias": perc_emergencias,
        "total_skips": total_skips,
        "ciclos_com_skip": ciclos_com_skip,
        "perc_ciclos_com_skip": perc_ciclos_com_skip,
        "total_embarked": total_embarked,
        "total_disembarked": total_disembarked,
        "top_floors": top_floors,
    }

    return metricas


# -------------------------------------------------------
# GERA RELATÓRIO TEXTO (POR LOG)
# -------------------------------------------------------

def salvar_relatorio_individual(metricas, summary_path: str, log_name: str):
    top_floors_str = ", ".join(
        f"andar {floor}: {count} paradas"
        for floor, count in metricas["top_floors"].items()
    ) if metricas["top_floors"] else "sem dados suficientes"

    texto = dedent(f"""
    RELATÓRIO DE QUALIDADE - {log_name}
    ===================================

    • Total de ciclos analisados: {metricas["total_ciclos"]}

    UTILIZAÇÃO DO ELEVADOR
    ----------------------
    • Ocupação média: {metricas["ocupacao_media"]:.1f} %
    • Ocupação máxima observada: {metricas["ocupacao_max"]:.1f} %
    • Ciclos com ocupação ≥ 80%: {metricas["ciclos_acima_80"]} 
      (equivalente a {metricas["perc_acima_80"]:.1f} % dos ciclos)

    SEGURANÇA OPERACIONAL
    ---------------------
    • Situações de EMERGÊNCIA detectadas: {metricas["total_emergencias"]} 
      (equivalente a {metricas["perc_emergencias"]:.2f} % dos ciclos)
    • Chamadas ignoradas (skipped_calls): {metricas["total_skips"]}
    • Ciclos com pelo menos uma chamada ignorada: {metricas["ciclos_com_skip"]} 
      (equivalente a {metricas["perc_ciclos_com_skip"]:.2f} % dos ciclos)

    FLUXO DE PASSAGEIROS
    --------------------
    • Eventos de embarque registrados: {metricas["total_embarked"]}
    • Passageiros desembarcados (estimados): {metricas["total_disembarked"]}

    PERFIL DE ATENDIMENTO POR ANDAR (DECISION_FLOOR)
    -----------------------------------------------
    • Andares mais atendidos: {top_floors_str}

    Observação:
    Estes dados são provenientes de simulação em bancada, com objetivo de estudo
    de critérios de qualidade, segurança e priorização de atendimento. 
    Não representam operação real de campo, mas fornecem base quantitativa para
    análise de risco, dimensionamento de carga e melhoria de lógica de atendimento.
    """)

    with open(summary_path, "w", encoding="utf-8") as f:
        f.write(texto)

    print(f"✔ Summary: {summary_path}")


# -------------------------------------------------------
# RELATÓRIO CONSOLIDADO
# -------------------------------------------------------

def salvar_relatorio_consolidado(resumos):
    if not resumos:
        print("Nenhuma métrica consolidada para salvar.")
        return

    linhas = []
    linhas.append("RELATÓRIO CONSOLIDADO - SMARTSTOP ELEVATOR SIMULATOR")
    linhas.append("====================================================")
    linhas.append("")
    linhas.append("Resumo por arquivo de log:\n")

    for r in resumos:
        top_floors_str = ", ".join(
            f"andar {floor}: {count} paradas"
            for floor, count in r["metricas"]["top_floors"].items()
        ) if r["metricas"]["top_floors"] else "sem dados suficientes"

        m = r["metricas"]
        linhas.append(f"Arquivo: {r['log_name']}")
        linhas.append(f"  • Ciclos: {m['total_ciclos']}")
        linhas.append(f"  • Ocupação média: {m['ocupacao_media']:.1f} %")
        linhas.append(f"  • Ocupação máxima: {m['ocupacao_max']:.1f} %")
        linhas.append(f"  • Ciclos com ≥80%% de ocupação: {m['ciclos_acima_80']} "
                      f"({m['perc_acima_80']:.1f} %)")
        linhas.append(f"  • Emergências: {m['total_emergencias']} "
                      f"({m['perc_emergencias']:.2f} %)")
        linhas.append(f"  • Chamadas ignoradas (skips): {m['total_skips']}, "
                      f"em {m['ciclos_com_skip']} ciclos "
                      f"({m['perc_ciclos_com_skip']:.2f} %)")
        linhas.append(f"  • Embarques: {m['total_embarked']}")
        linhas.append(f"  • Desembarques estimados: {m['total_disembarked']}")
        linhas.append(f"  • Andares mais atendidos: {top_floors_str}")
        linhas.append("")

    with open(CONSOLIDATED_OUTPUT, "w", encoding="utf-8") as f:
        f.write("\n".join(linhas))

    print(f"\n✔ Relatório consolidado salvo em:\n{CONSOLIDATED_OUTPUT}")


# -------------------------------------------------------
# MAIN
# -------------------------------------------------------

def main():
    print(">>> Procurando arquivos de log em:", BASE_DIR)

    # Procura logs .txt que provavelmente são de simulação
    candidatos = glob.glob(os.path.join(BASE_DIR, "*.txt"))

    log_files = []
    for path in candidatos:
        name = os.path.basename(path).lower()
        if ("log" in name) and ("data" not in name) and ("summary" not in name):
            log_files.append(path)

    if not log_files:
        print("Nenhum arquivo de log encontrado (com 'log' no nome).")
        return

    print(">>> Logs encontrados:")
    for lf in log_files:
        print("   -", os.path.basename(lf))

    resumos = []

    for log_path in log_files:
        log_name = os.path.basename(log_path)
        base_name = os.path.splitext(log_name)[0]

        print(f"\n=== Processando log: {log_name} ===")

        dados = parse_log(log_path)
        if not dados:
            print("  (sem dados válidos, ignorando este log)")
            continue

        csv_path = os.path.join(BASE_DIR, f"{base_name}_data.csv")
        xlsx_path = os.path.join(BASE_DIR, f"{base_name}_data.xlsx")
        summary_path = os.path.join(BASE_DIR, f"{base_name}_summary.txt")

        df = save_tables(dados, csv_path, xlsx_path)
        metricas = calcular_metricas(df)
        salvar_relatorio_individual(metricas, summary_path, log_name)

        resumos.append({
            "log_name": log_name,
            "metricas": metricas,
        })

    # Cria relatório consolidado
    salvar_relatorio_consolidado(resumos)

    print("\n🎉 Análise concluída com sucesso para todos os logs.")


if __name__ == "__main__":
    main()

