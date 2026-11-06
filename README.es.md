# cortex9-brotli-uncompressed-copy-narrowing-model

Modelo aritmético, en C, de **CVE-2020-8927** en el descompresor Brotli.
Antes del commit 223d80c, `CopyUncompressedBlockToOutput` guardaba en un
`int` los bytes de entrada restantes (`size_t`). Con 2 GiB o más de
entrada el valor se vuelve negativo, pasa los dos recortes y vuelve a
`size_t` como una longitud de copia enorme.

Es un modelo educativo y offline. No lee entrada, no descomprime, no
enlaza Brotli y no copia bytes. El ring buffer es una región abstracta de
`ringbuffer_size + 42` bytes.

## Fuente

- **El fallo:** `decode.c:1366-1373` (padre 0c5603e).
- **El fix:** solo cambia `bit_reader.h:90-94`, que pasa a devolver como
  máximo 2^30.
- **Versiones:** NVD dice "anteriores a 1.0.8". v1.0.8 y v1.0.9 contienen
  223d80c y se publicaron con nueve minutos de diferencia, así que no hay
  contradicción.

## Resultados

- **Antes del fix:** con size_t de 64 bits, 360 de 1458 casos desbordan
  el destino (mínimo: exactamente 2^31) y 222 se quedan atascados por
  truncamiento. Con 32 bits, 234 de 990 casos desbordan.
- **Fix y control `size_t_min`:** 0 casos que desborden y 0 atascados.
- **Comprobación aleatoria:** en 200.000 casos, el código previo al fix
  desborda si y solo si el cast da negativo.
- **Segunda implementación:** `scripts/derive_expected.py` (en Python)
  reproduce todos los resultados.

Ver [LIMITATIONS.md](LIMITATIONS.md), [SOURCES.md](SOURCES.md) y
[PRACTICE.md](PRACTICE.md). Licencia: Apache-2.0.
