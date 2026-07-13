#include "mikupan/ui/mikupan_rml_message_box.h"

#include "enums.h"
#include "graphics/graph2d/tim2.h"
#include "mikupan/mikupan_memory.h"
#include "mikupan/rendering/mikupan_gpu.h"

#include "RmlUi/Core.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace mikupan::rml_message_box
{

static constexpr float border_texture_tile_size = 192.0f;
static constexpr float border_texture_contrast = 1.65f;
static constexpr float border_texture_floor = 0.62f;

enum class FadeMode
{
    Solid,
    Left,
    Right
};

struct ElementData
{
    Rml::CompiledGeometryHandle geometry = 0;
    Rml::BoxArea paint_area = Rml::BoxArea::Padding;
};

class DecoratorInstancer;

struct State
{
    Rml::RenderInterface* render_interface = nullptr;
    std::array<std::unique_ptr<DecoratorInstancer>, 3> instancers;
    unsigned int texture_id = 0;
    bool initialized = false;
};

State g_state;

Rml::ColourbPremultiplied MakeBorderColour(std::uint8_t alpha)
{
    static constexpr int red = 0x8f;
    static constexpr int green = 0x73;
    static constexpr int blue = 0x4c;

    return {
        static_cast<std::uint8_t>((red * alpha + 127) / 255),
        static_cast<std::uint8_t>((green * alpha + 127) / 255),
        static_cast<std::uint8_t>((blue * alpha + 127) / 255),
        alpha};
}

float GetTextureSignal(std::uint32_t color)
{
    const float red = static_cast<float>(color & 0xffu) / 255.0f;
    const float green =
        static_cast<float>((color >> 16u) & 0xffu) / 255.0f;
    const float blue =
        static_cast<float>((color >> 8u) & 0xffu) / 255.0f;
    const float luminance =
        red * 0.2126f + green * 0.7152f + blue * 0.0722f;
    const float alpha = static_cast<float>(
        std::min(255u, ((color >> 24u) & 0xffu) * 2u))
        / 255.0f;
    return luminance * 0.7f + alpha * 0.3f;
}

unsigned int CreateTexture(void)
{
    static constexpr int texture_index = 5;
    static constexpr std::uint32_t ps2_memory_size = 32u * 1024u * 1024u;
    static constexpr std::uint32_t package_address_offset =
        PL_STTS_PK2_ADDRESS;
    static constexpr std::uint32_t package_limit =
        ps2_memory_size - package_address_offset;

    const std::int64_t package_address =
        MikuPan_GetHostAddress(PL_STTS_PK2_ADDRESS);
    if (package_address <= 0)
    {
        return 0;
    }

    const auto* package = reinterpret_cast<const std::uint32_t*>(
        package_address);
    const int texture_count = static_cast<int>(package[0]);
    if (texture_count <= texture_index || texture_count > 256)
    {
        return 0;
    }

    const std::uint32_t offset = package[4 + texture_index];
    if (offset < 16 || offset >= package_limit)
    {
        return 0;
    }

    void* tim2 = reinterpret_cast<void*>(package_address + offset);
    if (Tim2CheckFileHeaer(tim2) == 0)
    {
        return 0;
    }

    TIM2_PICTUREHEADER* picture = Tim2GetPictureHeader(tim2, 0);
    if (picture == nullptr || picture->TotalSize == 0
        || picture->TotalSize > package_limit - offset
        || picture->ImageWidth == 0 || picture->ImageHeight == 0
        || picture->ImageWidth > 4096 || picture->ImageHeight > 4096)
    {
        return 0;
    }

    const int width = picture->ImageWidth;
    const int height = picture->ImageHeight;
    float min_signal = 1.0f;
    float max_signal = 0.0f;

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            const std::uint32_t color =
                Tim2GetTextureColor(picture, 0, 0, x, y);
            const float signal = GetTextureSignal(color);
            min_signal = std::min(min_signal, signal);
            max_signal = std::max(max_signal, signal);
        }
    }

    std::vector<unsigned char> pixels(
        static_cast<std::size_t>(width)
        * static_cast<std::size_t>(height) * 4u);
    const float signal_range = max_signal - min_signal;

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            const std::uint32_t color =
                Tim2GetTextureColor(picture, 0, 0, x, y);
            const float signal = GetTextureSignal(color);
            const float normalized = signal_range > 0.0001f
                ? (signal - min_signal) / signal_range
                : 1.0f;
            const float contrasted = std::clamp(
                (normalized - 0.5f) * border_texture_contrast + 0.5f,
                0.0f,
                1.0f);
            const float modulation = border_texture_floor
                + contrasted * (1.0f - border_texture_floor);
            const auto channel = static_cast<unsigned char>(
                std::clamp(modulation * 255.0f, 0.0f, 255.0f));
            const std::size_t destination =
                static_cast<std::size_t>((y * width + x) * 4);
            pixels[destination + 0] = channel;
            pixels[destination + 1] = channel;
            pixels[destination + 2] = channel;
            pixels[destination + 3] = 255;
        }
    }

    return MikuPan_GPUCreateTextureRGBA8(
        width,
        height,
        pixels.data(),
        width * 4,
        1,
        0);
}

unsigned int EnsureTexture(void)
{
    if (g_state.texture_id == 0)
    {
        g_state.texture_id = CreateTexture();
    }
    return g_state.texture_id;
}

class Decorator final : public Rml::Decorator
{
public:
    explicit Decorator(FadeMode in_fade_mode)
        : fade_mode(in_fade_mode)
    {
    }

    Rml::DecoratorDataHandle GenerateElementData(
        Rml::Element* element,
        Rml::BoxArea paint_area) const override
    {
        if (element == nullptr || g_state.render_interface == nullptr)
        {
            return 0;
        }

        const Rml::Vector2f dimensions = element->GetBox().GetSize(paint_area);
        if (dimensions.x <= 0.0f || dimensions.y <= 0.0f)
        {
            return 0;
        }

        const Rml::Vector2f absolute_offset =
            element->GetAbsoluteOffset(paint_area);
        const float u0 = absolute_offset.x / border_texture_tile_size;
        const float v0 = absolute_offset.y / border_texture_tile_size;
        const float u1 =
            (absolute_offset.x + dimensions.x) / border_texture_tile_size;
        const float v1 =
            (absolute_offset.y + dimensions.y) / border_texture_tile_size;

        std::uint8_t left_alpha = 255;
        std::uint8_t right_alpha = 255;
        if (fade_mode == FadeMode::Left)
        {
            left_alpha = 0;
        }
        else if (fade_mode == FadeMode::Right)
        {
            right_alpha = 0;
        }

        std::array<Rml::Vertex, 4> vertices{};
        vertices[0].position = {0.0f, 0.0f};
        vertices[0].tex_coord = {u0, v0};
        vertices[0].colour = MakeBorderColour(left_alpha);

        vertices[1].position = {0.0f, dimensions.y};
        vertices[1].tex_coord = {u0, v1};
        vertices[1].colour = MakeBorderColour(left_alpha);

        vertices[2].position = {dimensions.x, dimensions.y};
        vertices[2].tex_coord = {u1, v1};
        vertices[2].colour = MakeBorderColour(right_alpha);

        vertices[3].position = {dimensions.x, 0.0f};
        vertices[3].tex_coord = {u1, v0};
        vertices[3].colour = MakeBorderColour(right_alpha);

        static constexpr std::array<int, 6> indices = {0, 1, 2, 0, 2, 3};
        const Rml::CompiledGeometryHandle geometry =
            g_state.render_interface->CompileGeometry(
                Rml::Span<const Rml::Vertex>(vertices.data(), vertices.size()),
                Rml::Span<const int>(indices.data(), indices.size()));
        if (geometry == 0)
        {
            return 0;
        }

        auto* element_data = new ElementData();
        element_data->geometry = geometry;
        element_data->paint_area = paint_area;
        return reinterpret_cast<Rml::DecoratorDataHandle>(element_data);
    }

    void ReleaseElementData(Rml::DecoratorDataHandle element_data) const override
    {
        auto* data = reinterpret_cast<ElementData*>(element_data);
        if (data == nullptr)
        {
            return;
        }

        if (g_state.render_interface != nullptr && data->geometry != 0)
        {
            g_state.render_interface->ReleaseGeometry(data->geometry);
        }
        delete data;
    }

    void RenderElement(
        Rml::Element* element,
        Rml::DecoratorDataHandle element_data) const override
    {
        auto* data = reinterpret_cast<ElementData*>(element_data);
        if (element == nullptr || data == nullptr
            || g_state.render_interface == nullptr)
        {
            return;
        }

        const unsigned int texture_id = EnsureTexture();
        g_state.render_interface->RenderGeometry(
            data->geometry,
            element->GetAbsoluteOffset(data->paint_area),
            static_cast<Rml::TextureHandle>(
                static_cast<std::uintptr_t>(texture_id)));
    }

private:
    FadeMode fade_mode;
};

class DecoratorInstancer final : public Rml::DecoratorInstancer
{
public:
    explicit DecoratorInstancer(FadeMode in_fade_mode)
        : fade_mode(in_fade_mode)
    {
    }

    Rml::SharedPtr<Rml::Decorator> InstanceDecorator(
        const Rml::String& name,
        const Rml::PropertyDictionary& properties,
        const Rml::DecoratorInstancerInterface& instancer_interface) override
    {
        (void) name;
        (void) properties;
        (void) instancer_interface;
        return Rml::SharedPtr<Rml::Decorator>(new Decorator(fade_mode));
    }

private:
    FadeMode fade_mode;
};

bool RegisterInstancer(
    std::size_t index,
    const char* name,
    FadeMode fade_mode)
{
    auto instancer = std::make_unique<DecoratorInstancer>(fade_mode);
    Rml::Factory::RegisterDecoratorInstancer(name, instancer.get());
    g_state.instancers[index] = std::move(instancer);
    return true;
}

}

bool MikuPan_RmlMessageBoxInit(Rml::RenderInterface* render_interface)
{
    if (mikupan::rml_message_box::g_state.initialized)
    {
        return true;
    }
    if (render_interface == nullptr)
    {
        return false;
    }

    mikupan::rml_message_box::g_state.render_interface = render_interface;
    const bool solid_registered = mikupan::rml_message_box::RegisterInstancer(
        0,
        "mikupan-message-border",
        mikupan::rml_message_box::FadeMode::Solid);
    const bool left_registered = mikupan::rml_message_box::RegisterInstancer(
        1,
        "mikupan-message-border-left",
        mikupan::rml_message_box::FadeMode::Left);
    const bool right_registered = mikupan::rml_message_box::RegisterInstancer(
        2,
        "mikupan-message-border-right",
        mikupan::rml_message_box::FadeMode::Right);

    mikupan::rml_message_box::g_state.initialized =
        solid_registered && left_registered && right_registered;
    return mikupan::rml_message_box::g_state.initialized;
}

void MikuPan_RmlMessageBoxShutdown(void)
{
    if (mikupan::rml_message_box::g_state.texture_id != 0)
    {
        MikuPan_GPUReleaseTexture(
            mikupan::rml_message_box::g_state.texture_id);
    }
    mikupan::rml_message_box::g_state = mikupan::rml_message_box::State();
}
