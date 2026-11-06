# Guía práctica (8 niveles)

Ejercicios **locales** de aritmética: no se lee ni se descomprime nada.
Cada respuesta se comprueba en `tests/test_model.c` (`practice_level_N`,
con ASan/UBSan).

Comando: `./build/brcopy case <pre|fix|size_t_min> AVAIL ACC META WBITS
POS 64|32`, después de `sh scripts/build.sh results /tmp/r`.

1. **Bloque pequeño.** `case pre 100 0 100 16 0 64`. (`practice_level_1`:
   SUCCESS, 100 bytes)
2. **Justo por debajo de 2 GiB.** `AVAIL=2147483647`, `META=4096`. ¿Qué
   recorte actúa? (`practice_level_2`: meta, 4096)
3. **El cast.** ¿Qué es `(int)2147483648` con GCC? Lee C11 6.3.1.3p3.
   (`practice_level_3`: INT_MIN)
4. **Los recortes no lo ven.** `case pre 2147483648 0 4096 16 0 64`.
   ¿Qué longitud `size_t` resulta? (`practice_level_4`:
   18446744071562067968, DEST_OVERRUN)
5. **El acumulador.** `AVAIL=2147483640 ACC=8`: ¿por qué también falla?
   (`practice_level_5`: 2^31 en total)
6. **Truncamiento positivo.** `AVAIL=4294967301`. (`practice_level_6`:
   copia 5 bytes y se atasca con entrada pendiente)
7. **El fix.** Repite el nivel 4 con `fix`. (`practice_level_7`:
   remaining = 2^30, SUCCESS)
8. **Control.** Repite el nivel 4 con `size_t_min`. (`practice_level_8`:
   recortar en `size_t` sin cast también es seguro)

*English summary: eight local levels from the cast to the fix and the
size_t control; every answer is asserted by the tests.*
