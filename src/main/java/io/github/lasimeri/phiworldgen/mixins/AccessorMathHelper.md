# AccessorMathHelper.java

Exposes `MathHelper.SIN_TABLE`, the 65,536 floats behind
`MathHelper.sin` and `cos`, which the caves and ravines call on every step.
The table is filled at class load with `(float) Math.sin(i * PI * 2 / 65536)`,
and `Math.sin` may differ between JVMs by an ulp, so the capture records the
running JVM's table and the card uses that, rather than recomputing it.
