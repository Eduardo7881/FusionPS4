#include "sce/gnm/GnmTypes.hpp"

namespace fusionps4::sce::gnm {

graphics::Format gnmTextureFormatToGal(GnmTextureFormat f) {
    using F = graphics::Format;
    switch (f) {
        case kGnmTextureFormatB8G8R8A8Unorm:     return F::B8G8R8A8_Unorm;
        case kGnmTextureFormatB8G8R8A8Srgb:      return F::B8G8R8A8_Srgb;
        case kGnmTextureFormatR8G8B8A8Unorm:     return F::R8G8B8A8_Unorm;
        case kGnmTextureFormatR8G8B8A8Srgb:      return F::R8G8B8A8_Srgb;
        case kGnmTextureFormatR16G16B16A16Float: return F::R16G16B16A16_Sfloat;
        case kGnmTextureFormatR32G32B32A32Float: return F::R32G32B32A32_Sfloat;
        case kGnmTextureFormatD32Float:          return F::D32_Sfloat;
        default:                                 return F::Unknown;
    }
}

} // namespace fusionps4::sce::gnm
