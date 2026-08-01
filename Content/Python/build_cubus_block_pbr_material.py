import unreal

ASSET_PATH = "/Game/Cubus/Materials"
MATERIAL_NAME = "M_CubusBlockPBR"
DEFAULT_COLOR = "/Engine/EngineResources/DefaultTexture.DefaultTexture"
DEFAULT_NORMAL = "/Engine/EngineResources/DefaultNormal.DefaultNormal"


def create_expression(material, expression_class, x, y):
    node = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        expression_class,
        x,
        y,
    )
    if node is None:
        raise RuntimeError(f"Could not create {expression_class}")
    return node


def connect(source, source_output, target, target_input):
    unreal.MaterialEditingLibrary.connect_material_expressions(
        source,
        source_output,
        target,
        target_input,
    )


def scalar(material, name, default, x, y):
    node = create_expression(material, unreal.MaterialExpressionScalarParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", default)
    return node


def vector(material, name, default, x, y):
    node = create_expression(material, unreal.MaterialExpressionVectorParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", default)
    return node


def constant(material, value, x, y):
    node = create_expression(material, unreal.MaterialExpressionConstant, x, y)
    node.set_editor_property("r", value)
    return node


def texture_sample(material, name, sampler_type, default_path, x, y):
    node = create_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("sampler_type", sampler_type)

    texture = unreal.EditorAssetLibrary.load_asset(default_path)
    if texture is None:
        raise RuntimeError(f"Missing engine default texture: {default_path}")

    node.set_editor_property("texture", texture)
    return node


def binary(material, expression_class, a, b, x, y, a_output="", b_output=""):
    node = create_expression(material, expression_class, x, y)
    connect(a, a_output, node, "A")
    connect(b, b_output, node, "B")
    return node


def multiply(material, a, b, x, y, a_output="", b_output=""):
    return binary(
        material,
        unreal.MaterialExpressionMultiply,
        a,
        b,
        x,
        y,
        a_output,
        b_output,
    )


def add(material, a, b, x, y, a_output="", b_output=""):
    return binary(
        material,
        unreal.MaterialExpressionAdd,
        a,
        b,
        x,
        y,
        a_output,
        b_output,
    )


def subtract(material, a, b, x, y, a_output="", b_output=""):
    return binary(
        material,
        unreal.MaterialExpressionSubtract,
        a,
        b,
        x,
        y,
        a_output,
        b_output,
    )


def lerp(material, a, b, alpha, x, y, a_output="", b_output="", alpha_output=""):
    node = create_expression(material, unreal.MaterialExpressionLinearInterpolate, x, y)
    connect(a, a_output, node, "A")
    connect(b, b_output, node, "B")
    connect(alpha, alpha_output, node, "Alpha")
    return node


def saturate(material, source, x, y, source_output=""):
    node = create_expression(material, unreal.MaterialExpressionSaturate, x, y)
    connect(source, source_output, node, "Input")
    return node


def one_minus(material, source, x, y, source_output=""):
    node = create_expression(material, unreal.MaterialExpressionOneMinus, x, y)
    connect(source, source_output, node, "Input")
    return node


def absolute(material, source, x, y, source_output=""):
    node = create_expression(material, unreal.MaterialExpressionAbs, x, y)
    connect(source, source_output, node, "Input")
    return node


def build_surface(material, prefix, uv, x, y):
    base = texture_sample(
        material,
        f"{prefix}BaseColor",
        unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,
        DEFAULT_COLOR,
        x,
        y,
    )
    normal = texture_sample(
        material,
        f"{prefix}Normal",
        unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
        DEFAULT_NORMAL,
        x,
        y + 220,
    )
    orm = texture_sample(
        material,
        f"{prefix}ORM",
        unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR,
        DEFAULT_COLOR,
        x,
        y + 440,
    )
    height = texture_sample(
        material,
        f"{prefix}Height",
        unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE,
        DEFAULT_COLOR,
        x,
        y + 660,
    )

    for node in (base, normal, orm, height):
        connect(uv, "", node, "UVs")

    return {
        "base": base,
        "normal": normal,
        "orm": orm,
        "height": height,
    }


def build_face_masks(material, selector):
    four = constant(material, 4.0, -2120, 860)
    half = constant(material, 0.5, -2120, 960)
    three_quarters = constant(material, 0.75, -2120, 1060)

    # Side selector is exactly 0.0. This becomes 1 at zero and falls to zero
    # before the top selector value of 0.5.
    side_scaled = multiply(
        material,
        selector,
        four,
        -1880,
        820,
        "A",
    )
    side_mask = saturate(
        material,
        one_minus(material, side_scaled, -1660, 820),
        -1440,
        820,
    )

    # Top selector is exactly 0.5. Build a triangular pulse centred on 0.5.
    top_delta = subtract(
        material,
        selector,
        half,
        -1880,
        960,
        "A",
    )
    top_abs = absolute(material, top_delta, -1660, 960)
    top_scaled = multiply(material, top_abs, four, -1440, 960)
    top_mask = saturate(
        material,
        one_minus(material, top_scaled, -1220, 960),
        -1000,
        960,
    )

    # Bottom selector is 1.0. Values below 0.75 are rejected.
    bottom_delta = subtract(
        material,
        selector,
        three_quarters,
        -1880,
        1100,
        "A",
    )
    bottom_scaled = multiply(material, bottom_delta, four, -1660, 1100)
    bottom_mask = saturate(material, bottom_scaled, -1440, 1100)

    return side_mask, top_mask, bottom_mask


def build_material():
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    full_path = f"{ASSET_PATH}/{MATERIAL_NAME}"

    material = unreal.EditorAssetLibrary.load_asset(full_path)
    if material is None:
        material = asset_tools.create_asset(
            MATERIAL_NAME,
            ASSET_PATH,
            unreal.Material,
            unreal.MaterialFactoryNew(),
        )

    if material is None:
        raise RuntimeError(f"Unable to create {full_path}")

    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    material.set_editor_property("two_sided", False)

    texcoord = create_expression(material, unreal.MaterialExpressionTextureCoordinate, -2800, -180)
    texture_scale = scalar(material, "TextureScale", 1.0, -2800, 20)
    uv = multiply(material, texcoord, texture_scale, -2550, -160)

    vertex_color = create_expression(material, unreal.MaterialExpressionVertexColor, -2800, 500)
    selector_passthrough = multiply(
        material,
        vertex_color,
        constant(material, 1.0, -2550, 660),
        -2300,
        520,
        "A",
    )

    side_mask, top_mask, bottom_mask = build_face_masks(material, selector_passthrough)

    side = build_surface(material, "Side", uv, -1900, -900)
    top = build_surface(material, "Top", uv, -1500, -900)
    bottom = build_surface(material, "Bottom", uv, -1100, -900)

    blend_start = scalar(material, "SideTopBlendStart", 0.70, -1900, 1320)
    blend_sharpness = scalar(material, "SideTopBlendSharpness", 4.0, -1900, 1440)

    uv_minus_start = subtract(
        material,
        uv,
        blend_start,
        -1640,
        1320,
        "G",
    )
    sharpened = multiply(material, uv_minus_start, blend_sharpness, -1400, 1320)
    side_band = saturate(material, sharpened, -1160, 1320)
    side_top_alpha = multiply(material, side_band, side_mask, -920, 1320)

    height_strength = scalar(material, "HeightStrength", 0.25, -1160, 1500)
    height_difference = subtract(
        material,
        top["height"],
        side["height"],
        -900,
        1500,
        "R",
        "R",
    )
    height_bias = multiply(material, height_difference, height_strength, -660, 1500)
    biased_alpha = add(material, side_top_alpha, height_bias, -420, 1380)
    final_side_alpha = multiply(
        material,
        saturate(material, biased_alpha, -180, 1380),
        side_mask,
        60,
        1380,
    )

    side_base = lerp(
        material,
        side["base"],
        top["base"],
        final_side_alpha,
        -680,
        -700,
        "RGB",
        "RGB",
    )
    side_normal = lerp(
        material,
        side["normal"],
        top["normal"],
        final_side_alpha,
        -680,
        -420,
        "RGB",
        "RGB",
    )
    side_orm = lerp(
        material,
        side["orm"],
        top["orm"],
        final_side_alpha,
        -680,
        -140,
        "RGB",
        "RGB",
    )

    top_base_masked = multiply(material, top["base"], top_mask, -420, -900, "RGB")
    bottom_base_masked = multiply(material, bottom["base"], bottom_mask, -420, -760, "RGB")
    side_base_masked = multiply(material, side_base, side_mask, -420, -620)
    final_base = add(
        material,
        add(material, top_base_masked, bottom_base_masked, -160, -840),
        side_base_masked,
        80,
        -760,
    )

    tint = vector(material, "Tint", unreal.LinearColor.WHITE, -160, -600)
    tinted_base = multiply(material, final_base, tint, 320, -760)

    top_normal_masked = multiply(material, top["normal"], top_mask, -420, -420, "RGB")
    bottom_normal_masked = multiply(material, bottom["normal"], bottom_mask, -420, -280, "RGB")
    side_normal_masked = multiply(material, side_normal, side_mask, -420, -140)
    final_normal = add(
        material,
        add(material, top_normal_masked, bottom_normal_masked, -160, -340),
        side_normal_masked,
        80,
        -260,
    )

    top_orm_masked = multiply(material, top["orm"], top_mask, -420, 40, "RGB")
    bottom_orm_masked = multiply(material, bottom["orm"], bottom_mask, -420, 180, "RGB")
    side_orm_masked = multiply(material, side_orm, side_mask, -420, 320)
    final_orm = add(
        material,
        add(material, top_orm_masked, bottom_orm_masked, -160, 120),
        side_orm_masked,
        80,
        200,
    )

    emissive_color = vector(material, "EmissiveColor", unreal.LinearColor.BLACK, 80, 500)
    emissive_strength = scalar(material, "EmissiveStrength", 0.0, 80, 620)
    emissive = multiply(material, emissive_color, emissive_strength, 320, 540)

    unreal.MaterialEditingLibrary.connect_material_property(
        tinted_base,
        "",
        unreal.MaterialProperty.MP_BASE_COLOR,
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        final_normal,
        "",
        unreal.MaterialProperty.MP_NORMAL,
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        final_orm,
        "R",
        unreal.MaterialProperty.MP_AMBIENT_OCCLUSION,
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        final_orm,
        "G",
        unreal.MaterialProperty.MP_ROUGHNESS,
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        final_orm,
        "B",
        unreal.MaterialProperty.MP_METALLIC,
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        emissive,
        "",
        unreal.MaterialProperty.MP_EMISSIVE_COLOR,
    )

    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
    unreal.log(f"Built and saved {full_path}")
    return material


if __name__ == "__main__":
    build_material()
