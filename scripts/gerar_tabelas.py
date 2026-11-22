from pathlib import Path
import textwrap
import pandas as pd
import matplotlib.pyplot as plt


def encontrar_dir_resultados() -> Path:
    """Encontra a pasta 'resultados/' rodando tanto da raiz quanto de 'scripts/'."""
    candidatos = [
        Path.cwd() / "resultados",
        Path(__file__).resolve().parent.parent / "resultados",
        Path.cwd(),
    ]
    for pasta in candidatos:
        if (pasta / "c1_normal_50ms.csv").exists():
            return pasta
    raise FileNotFoundError(
        "Não foi possível encontrar os CSVs (ex.: c1_normal_50ms.csv). "
        "Verifique se os 6 arquivos estão na pasta 'resultados/'."
    )


def fmt_int(valor: int) -> str:
    return f"{int(valor):,}".replace(",", " ")


def fmt_dec(valor: float, casas: int = 2) -> str:
    return f"{valor:.{casas}f}".replace(".", ",")


CENARIOS = [
    ("C1", "Normal (sem netem)", "c1_normal_50ms.csv"),
    ("C2", "Normal (sem netem)", "c2_normal_200ms.csv"),
    ("C3", "delay 50ms 40ms", "c3_jitter_50ms.csv"),
    ("C4", "delay 50ms 40ms", "c4_jitter_200ms.csv"),
    ("C5", "loss 20% delay 50ms 40ms", "c5_perda20_50ms.csv"),
    ("C6", "loss 20% delay 50ms 40ms", "c6_perda20_1000ms.csv"),
]


def carregar_metricas(dir_resultados: Path):
    linhas_t1 = []
    metricas = {}

    for cid, rede, arquivo in CENARIOS:
        df = pd.read_csv(dir_resultados / arquivo)
        ultimo = df.iloc[-1]

        latencia = int(ultimo["latency_ms"])
        duracao = int(ultimo["elapsed_s"])
        pushed = int(ultimo["pushed"])
        lost = int(ultimo["lost"])
        late = int(ultimo["late"])
        dup = int(ultimo["duplicates"])
        total = pushed + lost

        perda_pct = (100.0 * lost / total) if total > 0 else 0.0
        perda_rede = lost - late
        perda_rede_pct = (100.0 * perda_rede / total) if total > 0 else 0.0
        perda_atraso_pct = (100.0 * late / total) if total > 0 else 0.0

        j_media = df["avg_jitter_ms"].mean()
        j_dp = df["avg_jitter_ms"].std()
        j_mediana = df["avg_jitter_ms"].median()
        j_min = df["avg_jitter_ms"].min()
        j_max = df["avg_jitter_ms"].max()

        info = {
            "id": cid,
            "rede": rede,
            "latencia": latencia,
            "duracao": duracao,
            "pushed": pushed,
            "lost": lost,
            "late": late,
            "dup": dup,
            "total": total,
            "perda_pct": perda_pct,
            "perda_rede": perda_rede,
            "perda_rede_pct": perda_rede_pct,
            "perda_atraso_pct": perda_atraso_pct,
            "j_media": j_media,
            "j_dp": j_dp,
            "j_mediana": j_mediana,
            "j_min": j_min,
            "j_max": j_max,
        }

        perda_str = f"{fmt_dec(perda_pct)}%"
        jitter_media_str = f"{fmt_dec(j_media)} ± {fmt_dec(j_dp)}"

        if cid == "C3":
            df_est = df[df["avg_jitter_ms"] > 15.0]
            t_inicio = int(df_est.iloc[0]["elapsed_s"])
            t_fim = int(df_est.iloc[-1]["elapsed_s"])
            j_est_media = df_est["avg_jitter_ms"].mean()
            j_est_dp = df_est["avg_jitter_ms"].std()

            dp = int(df_est.iloc[-1]["pushed"] - df_est.iloc[0]["pushed"])
            dl = int(df_est.iloc[-1]["lost"] - df_est.iloc[0]["lost"])
            dlate = int(df_est.iloc[-1]["late"] - df_est.iloc[0]["late"])
            perda_est_pct = 100.0 * dl / (dp + dl)
            late_sobre_lost_est = 100.0 * dlate / dl

            info.update({
                "t_netem_inicio": t_inicio,
                "t_netem_fim": t_fim,
                "j_antes": df[df["elapsed_s"] < t_inicio].iloc[-1]["avg_jitter_ms"],
                "j_ativacao": df_est.iloc[0]["avg_jitter_ms"],
                "j_est_media": j_est_media,
                "j_est_dp": j_est_dp,
                "perda_est_pct": perda_est_pct,
                "dl_est": dl,
                "dlate_est": dlate,
                "late_sobre_lost_est": late_sobre_lost_est,
            })

            perda_str = f"{fmt_dec(perda_pct)}%*"
            jitter_media_str = (
                f"{fmt_dec(j_media)} ± {fmt_dec(j_dp)}\n"
                f"({fmt_dec(j_est_media)} ± {fmt_dec(j_est_dp)}*)"
            )

        metricas[cid] = info

        linhas_t1.append({
            "Cenário": cid,
            "Rede (tc netem em enp37s0)": rede,
            "Latência\n(ms)": str(latencia),
            "Duração\n(s)": str(duracao),
            "Recebidos\n(pushed)": fmt_int(pushed),
            "Perdidos\n(lost)": fmt_int(lost),
            "Atrasados\n(late)": fmt_int(late),
            "Duplicados": fmt_int(dup),
            "Perda\nfinal (%)": perda_str,
            "Jitter médio\n± DP (ms)": jitter_media_str,
            "Jitter mediana\n[mín–máx] (ms)": f"{fmt_dec(j_mediana)} [{fmt_dec(j_min)}–{fmt_dec(j_max)}]",
        })

    df_t1 = pd.DataFrame(linhas_t1)

    c1, c2, c3, c4, c5, c6 = (metricas[k] for k in ["C1", "C2", "C3", "C4", "C5", "C6"])
    red_perda_c3_c4 = 100.0 * (c3["perda_pct"] - c4["perda_pct"]) / c3["perda_pct"]
    red_late_c3_c4 = 100.0 * (c3["late"] - c4["late"]) / c3["late"]
    late_pct_c3 = 100.0 * c3["late"] / c3["lost"]
    dif_pp_c5_c6 = c5["perda_pct"] - c6["perda_pct"]

    achado_1 = textwrap.fill(
        f"Jitter estável ({fmt_dec(c2['j_media'])}–{fmt_dec(c1['j_media'])} ms) e "
        "integridade quase total sem degradação artificial (late = 0).",
        width=55,
    )
    achado_2 = textwrap.fill(
        f"Em 50 ms, {fmt_dec(late_pct_c3, 1)}% das perdas foram por atraso; "
        "o buffer de 200 ms absorveu o jitter mantendo baixa latência.",
        width=55,
    )
    achado_3 = textwrap.fill(
        "Buffer de 1 000 ms zerou descartes por atraso (late = 0), "
        "convergindo ao piso físico de 20% do RTP/UDP sem retransmissão.",
        width=55,
    )

    df_t2 = pd.DataFrame([
        {
            "Dimensão Avaliada": "1. Linha de base\nEthernet → Wi-Fi",
            "Cenários": "C1 vs. C2\n(50 vs. 200 ms)",
            "Perda Real\nna Rede": f"{fmt_dec(c1['perda_rede_pct'])}% → {fmt_dec(c2['perda_rede_pct'])}%",
            "Descarte por\nAtraso (late)": "0 → 0\n(0,00%)",
            "Perda Total\nEfetiva": f"{fmt_dec(c1['perda_pct'])}% → {fmt_dec(c2['perda_pct'])}%",
            "Principal Achado": achado_1,
        },
        {
            "Dimensão Avaliada": "2. Absorção de\njitter puro (±40 ms)",
            "Cenários": "C3 vs. C4\n(50 vs. 200 ms)",
            "Perda Real\nna Rede": f"{fmt_dec(c3['perda_rede_pct'])}% → {fmt_dec(c4['perda_rede_pct'])}%",
            "Descarte por\nAtraso (late)": (
                f"{fmt_int(c3['late'])} → {fmt_int(c4['late'])}\n"
                f"(-{fmt_dec(red_late_c3_c4)}%)"
            ),
            "Perda Total\nEfetiva": (
                f"{fmt_dec(c3['perda_pct'])}%* → {fmt_dec(c4['perda_pct'])}%\n"
                f"(-{fmt_dec(red_perda_c3_c4)}%)"
            ),
            "Principal Achado": achado_2,
        },
        {
            "Dimensão Avaliada": "3. Perda 20% +\njitter (±40 ms)",
            "Cenários": "C5 vs. C6\n(50 vs. 1000 ms)",
            "Perda Real\nna Rede": f"{fmt_dec(c5['perda_rede_pct'])}% → {fmt_dec(c6['perda_rede_pct'])}%",
            "Descarte por\nAtraso (late)": (
                f"{fmt_int(c5['late'])} ({fmt_dec(c5['perda_atraso_pct'])}%)\n"
                f"→ {fmt_int(c6['late'])} (0,00%)"
            ),
            "Perda Total\nEfetiva": (
                f"{fmt_dec(c5['perda_pct'])}% → {fmt_dec(c6['perda_pct'])}%\n"
                f"(-{fmt_dec(dif_pp_c5_c6)} p.p.)"
            ),
            "Principal Achado": achado_3,
        },
    ])

    nota_c3 = (
        f"*Em C3, a regra tc netem foi ativada após o arranque do vídeo "
        f"(aos {c3['t_netem_inicio']} s, passando o jitter de {fmt_dec(c3['j_antes'])} ms para "
        f"{fmt_dec(c3['j_ativacao'])} ms). Isolando apenas a janela de regime estacionário "
        f"({c3['t_netem_inicio']} s a {c3['t_netem_fim']} s), o jitter médio foi de "
        f"{fmt_dec(c3['j_est_media'])} ± {fmt_dec(c3['j_est_dp'])} ms e a taxa de perda efetiva "
        f"foi de {fmt_dec(c3['perda_est_pct'])}% ({fmt_int(c3['dl_est'])} perdas, das quais "
        f"{fmt_int(c3['dlate_est'])} — ou {fmt_dec(c3['late_sobre_lost_est'])}% — por atraso)."
    )

    return df_t1, df_t2, metricas, nota_c3


def salvar_tabela_png(
    df: pd.DataFrame,
    caminho_saida: Path,
    titulo: str,
    larguras_colunas: list[float],
    alinhamentos: list[str],
    nota_rodape: str = "",
    figsize: tuple[float, float] = (15.5, 4.6),
    row_heights: list[float] | None = None,
    font_size: float = 9.0,
):
    fig, ax = plt.subplots(figsize=figsize, dpi=300)
    ax.set_position([0.01, 0.14 if nota_rodape else 0.03, 0.98, 0.74 if nota_rodape else 0.84])
    ax.axis("off")

    ax.set_title(titulo, fontsize=12.5, fontweight="bold", color="#1E1B4B", pad=12, loc="center")

    soma = sum(larguras_colunas)
    col_widths = [w / soma for w in larguras_colunas]

    tabela = ax.table(
        cellText=df.values,
        colLabels=df.columns,
        colWidths=col_widths,
        cellLoc="center",
        loc="center",
        bbox=[0.0, 0.0, 1.0, 1.0],
    )
    tabela.auto_set_font_size(False)
    tabela.set_fontsize(font_size)

    n_rows, _ = df.shape
    if row_heights is None:
        row_heights = [0.18] + [0.13] * n_rows

    for (r, c), cell in tabela.get_celld().items():
        cell.set_height(row_heights[r])
        cell.set_edgecolor("#CBD5E1")
        cell.set_linewidth(0.7)

        if r == 0:
            cell.set_facecolor("#1E3A8A")
            cell.set_text_props(weight="bold", color="white", ha="center", va="center")
        else:
            cell.set_facecolor("#F8FAFC" if r % 2 == 1 else "#EEF2FF")
            ha = alinhamentos[c]
            text = cell.get_text()
            text.set_ha(ha)
            text.set_va("center")
            text.set_color("#0F172A")
            if c == 0 or df.columns[c].startswith("Perda\nfinal") or df.columns[c].startswith("Perda Total"):
                text.set_weight("bold")
            if ha == "left":
                cell.PAD = 0.035
            elif ha == "right":
                cell.PAD = 0.05

    if nota_rodape:
        ax.text(
            0.0,
            -0.04,
            nota_rodape,
            transform=ax.transAxes,
            fontsize=8.3,
            color="#334155",
            style="italic",
            ha="left",
            va="top",
        )

    fig.savefig(caminho_saida, dpi=300, bbox_inches="tight", facecolor="white", pad_inches=0.16)
    plt.close(fig)


def limpar_quebras(texto: str) -> str:
    return " ".join(str(texto).split())


def exportar_markdown(df_t1: pd.DataFrame, df_t2: pd.DataFrame, nota_c3: str, caminho: Path):
    def df_para_md(df: pd.DataFrame) -> str:
        cols = [limpar_quebras(c) for c in df.columns]
        linhas = [
            "| " + " | ".join(cols) + " |",
            "| " + " | ".join(["---"] * len(cols)) + " |",
        ]
        for _, row in df.iterrows():
            vals = [limpar_quebras(v) for v in row.values]
            linhas.append("| " + " | ".join(vals) + " |")
        return "\n".join(linhas)

    conteudo = (
        "# Tabelas de Resultados — EduStream (`rtpjitterbuffer`)\n\n"
        "## Tabela 1 — Comparativa dos 6 cenários\n\n"
        f"{df_para_md(df_t1)}\n\n"
        f"*{nota_c3.lstrip('*')}*\n\n"
        "## Tabela 2 — Síntese dos principais achados\n\n"
        f"{df_para_md(df_t2)}\n"
    )
    caminho.write_text(conteudo, encoding="utf-8")


def exportar_latex(df_t1: pd.DataFrame, df_t2: pd.DataFrame, nota_c3: str, caminho: Path):
    def esc(s: str) -> str:
        return (
            limpar_quebras(s)
            .replace("%", r"\%")
            .replace("_", r"\_")
            .replace("±", r"$\pm$")
            .replace("→", r"$\rightarrow$")
            .replace("–", "--")
            .replace("—", "---")
        )

    linhas = [
        r"% Requer no preâmbulo: \usepackage{booktabs}, \usepackage{graphicx} e \usepackage{tabularx}",
        r"\begin{table*}[htbp]",
        r"  \centering",
        r"  \caption{Resultados comparativos do \texttt{rtpjitterbuffer} nos seis cenários experimentais (Desktop Ethernet \texttt{enp37s0} $\rightarrow$ Notebook Wi-Fi).}",
        r"  \label{tab:resultados_cenarios}",
        r"  \resizebox{\textwidth}{!}{%",
        r"  \begin{tabular}{llrrrrrrrrl}",
        r"    \toprule",
        r"    \textbf{Cen.} & \textbf{Rede (\texttt{tc netem})} & \textbf{Lat. (ms)} & \textbf{Dur. (s)} & \textbf{Receb.} & \textbf{Perd.} & \textbf{Atras.} & \textbf{Dup.} & \textbf{Perda (\%)} & \textbf{Jitter médio $\pm$ DP (ms)} & \textbf{Mediana [mín--máx] (ms)} \\",
        r"    \midrule",
    ]
    for _, row in df_t1.iterrows():
        linhas.append("    " + " & ".join(esc(v) for v in row.values) + r" \\")
    linhas += [
        r"    \bottomrule",
        r"  \end{tabular}}",
        r"  \vspace{4pt}",
        r"  \parbox{\textwidth}{\footnotesize \textit{" + esc(nota_c3) + r"}}",
        r"\end{table*}",
        "",
        r"\begin{table*}[htbp]",
        r"  \centering",
        r"  \caption{Síntese dos principais achados experimentais e decomposição de perdas (C1--C6).}",
        r"  \label{tab:principais_achados}",
        r"  \begin{tabularx}{\textwidth}{lccccX}",
        r"    \toprule",
        r"    \textbf{Dimensão} & \textbf{Cenários} & \textbf{Perda Real} & \textbf{Atraso (\texttt{late})} & \textbf{Perda Total} & \textbf{Principal Achado} \\",
        r"    \midrule",
    ]
    for _, row in df_t2.iterrows():
        linhas.append("    " + " & ".join(esc(v) for v in row.values) + r" \\")
    linhas += [
        r"    \bottomrule",
        r"  \end{tabularx}",
        r"\end{table*}",
    ]
    caminho.write_text("\n".join(linhas) + "\n", encoding="utf-8")


def main():
    dir_res = encontrar_dir_resultados()
    df_t1, df_t2, metricas, nota_c3 = carregar_metricas(dir_res)

    # 1. CSV consolidado
    df_t1_limpo = df_t1.rename(columns=limpar_quebras).apply(lambda col: col.map(limpar_quebras))
    df_t1_limpo.to_csv(dir_res / "tabela1_comparativa_6_cenarios.csv", index=False, encoding="utf-8-sig")

    # 2. Markdown e LaTeX (booktabs)
    exportar_markdown(df_t1, df_t2, nota_c3, dir_res / "tabelas_resultados.md")
    exportar_latex(df_t1, df_t2, nota_c3, dir_res / "tabelas_resultados.tex")

    # 3. Tabela 1 em PNG (300 DPI)
    c3 = metricas["C3"]
    nota_png = (
        f"*Em C3, a regra tc netem foi ativada após o arranque do vídeo "
        f"(aos {c3['t_netem_inicio']} s, passando o jitter de {fmt_dec(c3['j_antes'])} ms para "
        f"{fmt_dec(c3['j_ativacao'])} ms).\n"
        f"Isolando a janela de regime estacionário ({c3['t_netem_inicio']} s a {c3['t_netem_fim']} s), "
        f"o jitter médio foi de {fmt_dec(c3['j_est_media'])} ± {fmt_dec(c3['j_est_dp'])} ms e a perda "
        f"efetiva foi de {fmt_dec(c3['perda_est_pct'])}% ({fmt_int(c3['dl_est'])} perdas; "
        f"{fmt_int(c3['dlate_est'])} ou {fmt_dec(c3['late_sobre_lost_est'])}% por atraso)."
    )

    salvar_tabela_png(
        df=df_t1,
        caminho_saida=dir_res / "tabela1_comparativa_6_cenarios.png",
        titulo="Tabela 1 — Desempenho do rtpjitterbuffer sob jitter e perda de pacotes (6 cenários)",
        larguras_colunas=[0.6, 2.05, 0.8, 0.75, 0.95, 0.85, 0.85, 0.85, 0.9, 1.75, 1.8],
        alinhamentos=["center", "left", "right", "right", "right", "right", "right", "right", "right", "center", "center"],
        nota_rodape=nota_png,
        figsize=(16.0, 4.7),
        row_heights=[0.18, 0.12, 0.12, 0.17, 0.12, 0.12, 0.12],
        font_size=8.8,
    )

    # 4. Tabela 2 em PNG (300 DPI)
    salvar_tabela_png(
        df=df_t2,
        caminho_saida=dir_res / "tabela2_principais_achados.png",
        titulo="Tabela 2 — Síntese dos principais achados experimentais (C1–C6)",
        larguras_colunas=[1.55, 1.3, 1.3, 1.45, 1.45, 3.8],
        alinhamentos=["left", "center", "center", "center", "center", "left"],
        figsize=(15.0, 4.0),
        row_heights=[0.18, 0.27, 0.27, 0.28],
        font_size=8.8,
    )

    print(f"Arquivos gerados com sucesso em: {dir_res}")
    for nome in [
        "tabela1_comparativa_6_cenarios.png",
        "tabela2_principais_achados.png",
        "tabela1_comparativa_6_cenarios.csv",
        "tabelas_resultados.tex",
        "tabelas_resultados.md",
    ]:
        print(f"  - {nome}")


if __name__ == "__main__":
    main()
