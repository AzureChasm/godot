def can_build(env, platform):
    return not env["disable_3d"] and not env["disable_physics_3d"]


def configure(env):
    pass


def get_doc_classes():
    return ["RegionContainer", "TerrainView", "WorldDecorator", "WorldSphereDecorator"]


def get_doc_path():
    return "doc_classes"
