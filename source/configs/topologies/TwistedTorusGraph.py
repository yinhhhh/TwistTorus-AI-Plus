"""Pure graph helpers shared by the Twisted Torus config and checker."""


DIRECTIONS = (
    ("East", "West"),
    ("West", "East"),
    ("North", "South"),
    ("South", "North"),
    ("Up", "Down"),
    ("Down", "Up"),
)


def validate_dimensions(size_x, size_y, size_z, twist_mode):
    if min(size_x, size_y, size_z) < 2:
        raise ValueError("all Torus dimensions must be at least two")
    if twist_mode not in ("none", "single", "double", "synth", "custom"):
        raise ValueError(
            "twist_mode must be none, single, double, synth, or custom"
        )
    if twist_mode not in ("none", "custom") and not (
        size_x == 2 * size_y and size_y == size_z
    ):
        raise ValueError(
            "twisted modes require X x Y x Z = 2a x a x a"
        )


def router_id(x, y, z, size_x, size_y):
    return (z * size_y + y) * size_x + x


def coordinates(node_id, size_x, size_y):
    plane = size_x * size_y
    z, offset = divmod(node_id, plane)
    y, x = divmod(offset, size_x)
    return x, y, z


def directed_neighbors(
    node_id, size_x, size_y, size_z, twist_mode,
    custom_y_shift=0, custom_z_shift=0
):
    """Return (destination, source outport, destination inport)."""
    validate_dimensions(size_x, size_y, size_z, twist_mode)
    x, y, z = coordinates(node_id, size_x, size_y)
    y_shift = (
        custom_y_shift % size_x if twist_mode == "custom"
        else size_y - 1 if twist_mode == "synth" else size_y
    )
    z_shift = custom_z_shift % size_x if twist_mode == "custom" else size_z

    east = ((x + 1) % size_x, y, z)
    west = ((x - 1) % size_x, y, z)

    if y == size_y - 1 and twist_mode != "none":
        north = ((x + y_shift) % size_x, 0, z)
    else:
        north = (x, (y + 1) % size_y, z)
    if y == 0 and twist_mode != "none":
        south = ((x - y_shift) % size_x, size_y - 1, z)
    else:
        south = (x, (y - 1) % size_y, z)

    if z == size_z - 1 and twist_mode in ("double", "synth", "custom"):
        up = ((x + z_shift) % size_x, y, 0)
    else:
        up = (x, y, (z + 1) % size_z)
    if z == 0 and twist_mode in ("double", "synth", "custom"):
        down = ((x - z_shift) % size_x, y, size_z - 1)
    else:
        down = (x, y, (z - 1) % size_z)

    destinations = (east, west, north, south, up, down)
    return [
        (
            router_id(*destination, size_x, size_y),
            outport,
            inport,
        )
        for destination, (outport, inport) in zip(destinations, DIRECTIONS)
    ]
