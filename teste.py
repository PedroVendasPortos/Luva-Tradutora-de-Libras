import os
os.environ['TF_CPP_MIN_LOG_LEVEL'] = '3'  # Oculta avisos do C++ do TensorFlow

import tensorflow as tf

print("Versão do TensorFlow:", tf.__version__)
print("Testando constante:", tf.constant("TensorFlow instalado com sucesso!").numpy().decode())