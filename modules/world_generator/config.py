def can_build(env, platform):
    return not env["disable_3d"] and not env["disable_physics_3d"]


def configure(env):
    pass


def get_doc_classes():
    return [
        "RegionContainer",
        "WorldRegionLayout",
        "TerrainView",
        "WorldDecorator",
        "WorldSphereDecorator",
        "WorldTerrainDecorator",
        "WorldPlainsDecorator",
        "WorldHillsDecorator",
        "WorldMountainsDecorator",
        "WorldSeaDecorator",
        "WorldDeepSeaDecorator",
    ]


def get_doc_path():
    return "doc_classes"
