import unreal

ASSET_PATH = "/Game/Cubus/Materials"
MATERIAL_NAME = "M_CubusBlockPBR"


def create_expression(material, expression_class, x, y):
    return unreal.MaterialEditingLibrary.create_material_expression(
        material,
        expression_class,
        x,
        y,
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


def texture_sample(material, name, sampler_type, x, y):
    node = create_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("sampler_type", sampler_type)
    return node


def connect(source, source_output, target, target_input):
    unreal.MaterialEditingLibrary.connect_material_expressions(
        source,
        source_output,
        target,
        target_input,
    )


def multiply(material, a, b, x, y):
    node = create_expression(material, unreal.MaterialExpressionMultiply, x, y)
    connect(a, "", node, "A")
    connect(b, "", node, "B")
    return node


def add(material, a, b, x, y):
    node = create_expression(material, unreal.MaterialExpressionAdd, x, y)
    connect(a, "", node, "A")
    connect(b, "", node, "B")
    return node


def lerp(material, a, b, alpha, x, y):
    node = create_expression(material, unreal.MaterialExpressionLinearInterpolate, x, y)
    connect(a, "", node, "A")
    connect(b, "", node, "B")
    connect(alpha, "", node, "Alpha")
    return node


def component_mask(material, source, r, g, b, a, x, y):
    node = create_expression(material, unreal.MaterialExpressionComponentMask, x, y)
    node.set_editor_property("r", r)
    node.set_editor_property("g", g)
    node.set_editor_property("b", b)
    node.set_editor_property("a", a)
    connect(source, "", node, "Input")
    return node


def if_node(material, a, b, greater, equal, less, x, y):
    node = create_expression(material, unreal.MaterialExpressionIf, x, y)
    connect(a, "", node, "A")
    connect(b, "", node, "B")
    connect(greater, "", node, "AGreaterThanB")
    connect(equal, "", node, "AEqualsB")
    connect(less, "", node, "ALessThanB")
    return node


def build_surface(material, prefix, uv, x, y):
    base = texture_sample(material, f"{prefix}BaseColor", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, x, y)
    normal = texture_sample(material, f"{prefix}Normal", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, x, y + 220)
    orm = texture_sample(material, f"{prefix}ORM", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR, x, y + 440)
    height = texture_sample(material, f"{prefix}Height", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE, x, y + 660)
    for node in (base, normal, orm, height):
        connect(uv, "", node, "UVs")
    return {"base": base, "normal": normal, "orm": orm, "height": height}


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

    texcoord = create_expression(material, unreal.MaterialExpressionTextureCoordinate, -2600, -200)
    texture_scale = scalar(material, "TextureScale", 1.0, -2600, 20)
    uv = multiply(material, texcoord, texture_scale, -2350, -180)

    vertex_color = create_expression(material, unreal.MaterialExpressionVertexColor, -2600, 380)
    selector = component_mask(material, vertex_color, False, False, False, True, -2350, 380)

    zero = scalar(material, "SelectorSide", 0.0, -2350, 560)
    half = scalar(material, "SelectorTop", 0.5, -2350, 660)
    one = scalar(material, "SelectorBottom", 1.0, -2350, 760)

    const_zero = create_expression(material, unreal.MaterialExpressionConstant, -2100, 560)
    const_zero.set_editor_property("r", 0.0)
    const_one = create_expression(material, unreal.MaterialExpressionConstant, -2100, 660)
    const_one.set_editor_property("r", 1.0)

    top_mask = if_node(material, selector, half, const_zero, const_one, const_zero, -1850, 480)
    bottom_mask = if_node(material, selector, one, const_zero, const_one, const_zero, -1850, 680)
    side_mask = if_node(material, selector, zero, const_zero, const_one, const_zero, -1850, 880)

    side = build_surface(material, "Side", uv, -1800, -900)
    top = build_surface(material, "Top", uv, -1450, -900)
    bottom = build_surface(material, "Bottom", uv, -1100, -900)

    uv_y = component_mask(material, uv, False, True, False, False, -1800, 1200)
    blend_start = scalar(material, "SideTopBlendStart", 0.70, -1800, 1380)
    blend_sharpness = scalar(material, "SideTopBlendSharpness", 4.0, -1800, 1480)

    subtract = create_expression(material, unreal.MaterialExpressionSubtract, -1500, 1260)
    connect(uv_y, "", subtract, "A")
    connect(blend_start, "", subtract, "B")
    sharpened = multiply(material, subtract, blend_sharpness, -1260, 1260)
    saturate = create_expression(material, unreal.MaterialExpressionSaturate, -1040, 1260)
    connect(sharpened, "", saturate, "Input")
    side_top_alpha = multiply(material, saturate, side_mask, -820, 1260)

    height_strength = scalar(material, "HeightStrength", 0.25, -1040, 1460)
    height_difference = create_expression(material, unreal.MaterialExpressionSubtract, -820, 1460)
    connect(top["height"], "R", height_difference, "A")
    connect(side["height"], "R", height_difference, "B")
    height_bias = multiply(material, height_difference, height_strength, -600, 1460)
    height_alpha_add = add(material, side_top_alpha, height_bias, -380, 1320)
    height_alpha = create_expression(material, unreal.MaterialExpressionSaturate, -160, 1320)
    connect(height_alpha_add, "", height_alpha, "Input")
    final_side_alpha = multiply(material, height_alpha, side_mask, 60, 1320)

    side_base = lerp(material, side["base"], top["base"], final_side_alpha, -650, -700)
    side_normal = lerp(material, side["normal"], top["normal"], final_side_alpha, -650, -420)
    side_orm = lerp(material, side["orm"], top["orm"], final_side_alpha, -650, -140)

    top_base_masked = multiply(material, top["base"], top_mask, -360, -900)
    bottom_base_masked = multiply(material, bottom["base"], bottom_mask, -360, -760)
    side_base_masked = multiply(material, side_base, side_mask, -360, -620)
    final_base = add(material, add(material, top_base_masked, bottom_base_masked, -80, -840), side_base_masked, 140, -760)

    tint = vector(material, "Tint", unreal.LinearColor.WHITE, -80, -620)
    tinted_base = multiply(material, final_base, tint, 360, -760)

    top_normal_masked = multiply(material, top["normal"], top_mask, -360, -420)
    bottom_normal_masked = multiply(material, bottom["normal"], bottom_mask, -360, -280)
    side_normal_masked = multiply(material, side_normal, side_mask, -360, -140)
    final_normal = add(material, add(material, top_normal_masked, bottom_normal_masked, -80, -340), side_normal_masked, 140, -260)

    top_orm_masked = multiply(material, top["orm"], top_mask, -360, 40)
    bottom_orm_masked = multiply(material, bottom["orm"], bottom_mask, -360, 180)
    side_orm_masked = multiply(material, side_orm, side_mask, -360, 320)
    final_orm = add(material, add(material, top_orm_masked, bottom_orm_masked, -80, 120), side_orm_masked, 140, 200)

    ao = component_mask(material, final_orm, True, False, False, False, 380, 40)
    roughness = component_mask(material, final_orm, False, True, False, False, 380, 180)
    metallic = component_mask(material, final_orm, False, False, True, False, 380, 320)

    emissive_color = vector(material, "EmissiveColor", unreal.LinearColor.BLACK, 140, 500)
    emissive_strength = scalar(material, "EmissiveStrength", 0.0, 140, 620)
    emissive = multiply(material, emissive_color, emissive_strength, 380, 540)

    unreal.MaterialEditingLibrary.connect_material_property(tinted_base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(final_normal, "", unreal.MaterialProperty.MP_NORMAL)
    unreal.MaterialEditingLibrary.connect_material_property(ao, "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    unreal.MaterialEditingLibrary.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.connect_material_property(metallic, "", unreal.MaterialProperty.MP_METALLIC)
    unreal.MaterialEditingLibrary.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
    unreal.log(f"Built and saved {full_path}")
    return material


if __name__ == "__main__":
    build_material()
