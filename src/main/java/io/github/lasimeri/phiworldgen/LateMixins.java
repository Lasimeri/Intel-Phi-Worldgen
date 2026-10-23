package io.github.lasimeri.phiworldgen;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;

import com.gtnewhorizon.gtnhmixins.ILateMixinLoader;
import com.gtnewhorizon.gtnhmixins.LateMixin;

/** The mixins into other mods' classes, applied once those mods are loaded. See LateMixins.md. */
@LateMixin
public class LateMixins implements ILateMixinLoader {

    @Override
    public String getMixinConfig() {
        return "mixins.phiworldgen.late.json";
    }

    @Override
    public List<String> getMixins(Set<String> loadedMods) {
        List<String> mixins = new ArrayList<>();
        if (loadedMods.contains("RWG")) {
            mixins.add("AccessorChunkGeneratorRealistic");
            mixins.add("MixinChunkGeneratorRealistic_Profile");
            mixins.add("MixinChunkGeneratorRealistic_Card");
        }
        return mixins;
    }
}
