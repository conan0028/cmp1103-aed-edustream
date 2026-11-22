import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

plt.rcParams.update({
    "font.size": 10,
    "axes.spines.top": False,
    "axes.spines.right": False,
    "axes.grid": True,
    "grid.alpha": 0.25,
    "grid.linestyle": "--",
})

cenarios = [
    ("C1\nNormal\n50 ms", "resultados/c1_normal_50ms.csv"),
    ("C2\nNormal\n200 ms", "resultados/c2_normal_200ms.csv"),
    ("C3\nJitter\n50 ms", "resultados/c3_jitter_50ms.csv"),
    ("C4\nJitter\n200 ms", "resultados/c4_jitter_200ms.csv"),
    ("C5\nPerda 20%\n50 ms", "resultados/c5_perda20_50ms.csv"),
    ("C6\nPerda 20%\n1000 ms", "resultados/c6_perda20_1000ms.csv"),
]

labels, net_loss_pct, late_loss_pct, jitter_mean = [], [], [], []
dfs = {}

for label, path in cenarios:
    df = pd.read_csv(path)
    dfs[label] = df
    last = df.iloc[-1]
    total = last["pushed"] + last["lost"]
    late_p = 100.0 * last["late"] / total
    net_p = 100.0 * (last["lost"] - last["late"]) / total
    labels.append(label)
    net_loss_pct.append(net_p)
    late_loss_pct.append(late_p)
    jitter_mean.append(df["avg_jitter_ms"].mean())

# Figura 1: Perda decomposta (rede vs. atraso) + Jitter médio
fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(11.5, 4.4), dpi=300)
x = np.arange(len(labels))

b1 = ax1.bar(x, net_loss_pct, color="#2563EB", label="Perda real na rede (lost - late)")
b2 = ax1.bar(x, late_loss_pct, bottom=net_loss_pct, color="#9333EA", label="Descarte por atraso (late)")
for i, (n, l) in enumerate(zip(net_loss_pct, late_loss_pct)):
    ax1.text(i, n + l + 0.6, f"{n + l:.2f}%", ha="center", va="bottom", fontsize=8.5, fontweight="bold")

ax1.set_xticks(x)
ax1.set_xticklabels(labels, fontsize=8.5)
ax1.set_ylabel("Taxa de perda efetiva (%)")
ax1.set_ylim(0, 34)
ax1.set_title("(a) Decomposição da Perda de Pacotes")
ax1.legend(frameon=False, fontsize=8.5, loc="upper left")

ax2.bar(x, jitter_mean, color="#4F46E5")
for i, j in enumerate(jitter_mean):
    ax2.text(i, j + 0.6, f"{j:.2f} ms", ha="center", va="bottom", fontsize=8.5, fontweight="bold")
ax2.set_xticks(x)
ax2.set_xticklabels(labels, fontsize=8.5)
ax2.set_ylabel("Jitter médio RTP (ms)")
ax2.set_ylim(0, 36)
ax2.set_title("(b) Jitter Médio Medido no Recetor")

fig.tight_layout()
fig.savefig("resultados/fig1_perda_decomposta_jitter.png", dpi=300, bbox_inches="tight")
plt.close(fig)

# Figura 2: Evolução temporal da taxa de perda e do jitter nos cenários degradados
fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(11.5, 4.6), dpi=300)
palette = {
    "C2\nNormal\n200 ms": ("#64748B", "-", "C2 Normal (200 ms)"),
    "C3\nJitter\n50 ms": ("#0284C7", "-", "C3 Jitter (50 ms)"),
    "C4\nJitter\n200 ms": ("#2563EB", "--", "C4 Jitter (200 ms)"),
    "C5\nPerda 20%\n50 ms": ("#7C3AED", "-", "C5 Perda 20%+Jitter (50 ms)"),
    "C6\nPerda 20%\n1000 ms": ("#C026D3", "-.", "C6 Perda 20%+Jitter (1000 ms)"),
}

for key, (color, ls, leg) in palette.items():
    df = dfs[key]
    ax1.plot(df["elapsed_s"], df["loss_rate_pct"], label=leg, color=color, linestyle=ls, linewidth=1.8)
    ax2.plot(df["elapsed_s"], df["avg_jitter_ms"], label=leg, color=color, linestyle=ls, linewidth=1.8)

ax1.set_xlabel("Tempo decorrido (s)")
ax1.set_ylabel("Taxa de perda acumulada (%)")
ax1.set_title("(a) Evolução da Perda Acumulada")

ax2.set_xlabel("Tempo decorrido (s)")
ax2.set_ylabel("Jitter médio RTP (ms)")
ax2.set_title("(b) Evolução do Jitter ao Longo do Ensaio")

handles, leg_labels = ax1.get_legend_handles_labels()
fig.legend(
    handles,
    leg_labels,
    loc="lower center",
    bbox_to_anchor=(0.5, 0.01),
    ncol=3,
    frameon=False,
    fontsize=9,
)

fig.tight_layout(rect=[0, 0.11, 1, 1])
fig.savefig("resultados/fig2_evolucao_temporal.png", dpi=300, bbox_inches="tight")
plt.close(fig)
print("Figuras geradas em resultados/.")
