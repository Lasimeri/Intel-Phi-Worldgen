package io.github.lasimeri.phiworldgen.mixins;

import net.minecraft.util.MathHelper;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.gen.Accessor;

/** MathHelper's sine table, as this JVM computed it. See AccessorMathHelper.md. */
@Mixin(MathHelper.class)
public interface AccessorMathHelper {

    @Accessor("SIN_TABLE")
    static float[] phiwg$getSinTable() {
        throw new AssertionError();
    }
}
