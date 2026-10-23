package io.github.lasimeri.phiworldgen.latemixins;

import net.minecraft.world.gen.MapGenBase;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Pseudo;
import org.spongepowered.asm.mixin.gen.Accessor;

/** RWG's generator fields the census reads. See AccessorChunkGeneratorRealistic.md. */
@Pseudo
@Mixin(targets = "rwg.world.ChunkGeneratorRealistic", remap = false)
public interface AccessorChunkGeneratorRealistic {

    @Accessor(value = "caves", remap = false)
    MapGenBase phiwg$caves();
}
