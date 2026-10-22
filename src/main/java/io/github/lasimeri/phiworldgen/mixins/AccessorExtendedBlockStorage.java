package io.github.lasimeri.phiworldgen.mixins;

import net.minecraft.world.chunk.storage.ExtendedBlockStorage;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.gen.Accessor;

/** A section's two block counts, which the whole-chunk builder sets itself. See AccessorExtendedBlockStorage.md. */
@Mixin(ExtendedBlockStorage.class)
public interface AccessorExtendedBlockStorage {

    @Accessor("blockRefCount")
    int phiwg$getBlockRefCount();

    @Accessor("blockRefCount")
    void phiwg$setBlockRefCount(int n);

    @Accessor("tickRefCount")
    int phiwg$getTickRefCount();

    @Accessor("tickRefCount")
    void phiwg$setTickRefCount(int n);
}
