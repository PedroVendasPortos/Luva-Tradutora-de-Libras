"""Perfis iniciais e ajustáveis para o protótipo de cinco sensores.

Ordem: polegar, indicador, médio, anelar, mínimo.
Valores: 0 = estendido; 1 = dobrado; 0.5 = curvatura intermediária.
Estes perfis são hipóteses de bancada, não definições completas de Libras.
"""

FINGER_ORDER = ("polegar", "indicador", "medio", "anelar", "minimo")

GESTURE_PROFILES = {
    # P: dobra experimental apenas do indicador.
    "P": (0.0, 1.0, 0.0, 0.0, 0.0),
    # E: cinco dedos dobrados.
    "E": (1.0, 1.0, 1.0, 1.0, 1.0),
    # D: indicador estendido e os demais dobrados.
    "D": (1.0, 0.0, 1.0, 1.0, 1.0),
    # R: perfil simplificado; cruzamento dos dedos não é mensurável por flex.
    "R": (0.0, 0.0, 0.0, 0.0, 0.0),
    # O: polegar dobrado e demais dedos com curvatura intermediária.
    "O": (1.0, 0.5, 0.5, 0.5, 0.5),
}

# Limites de bancada, a ajustar após observar leituras reais.
MAX_DISTANCE = 0.38
MIN_MARGIN = 0.08
