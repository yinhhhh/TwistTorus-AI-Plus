from m5.objects import *
from m5.params import *

from common import FileSystemConfig
from topologies.BaseTopology import SimpleTopology
from topologies.TwistedTorusGraph import directed_neighbors, validate_dimensions


class TwistedTorus(SimpleTopology):
    """A parameterized 2a x a x a prismatic (doubly) twisted torus."""

    description = "TwistedTorus"

    def __init__(self, controllers):
        self.nodes = controllers

    def makeTopology(self, options, network, IntLink, ExtLink, Router):
        size_x = options.torus_x
        size_y = options.torus_y
        size_z = options.torus_z
        twist_mode = options.twist_mode
        custom_y_shift = options.twist_y_shift
        custom_z_shift = options.twist_z_shift
        validate_dimensions(size_x, size_y, size_z, twist_mode)

        num_routers = size_x * size_y * size_z
        if options.num_cpus != num_routers:
            raise ValueError(
                "TwistedTorus requires --num-cpus to equal "
                f"--torus-x * --torus-y * --torus-z ({num_routers})"
            )

        routers = [
            Router(router_id=i, latency=options.router_latency)
            for i in range(num_routers)
        ]
        network.routers = routers

        link_id = 0
        ext_links = []
        # Directory counts must be powers of two, whereas an arbitrary
        # rectangular torus may contain a non-power-of-two number of routers.
        # A round-robin attachment supports both without changing the internal
        # topology or adding router-to-router links.
        for index, node in enumerate(self.nodes):
            attached_router = index % num_routers
            ext_links.append(
                ExtLink(
                    link_id=link_id,
                    ext_node=node,
                    int_node=routers[attached_router],
                    latency=options.link_latency,
                )
            )
            link_id += 1
        network.ext_links = ext_links

        int_links = []
        for source in range(num_routers):
            for destination, outport, inport in directed_neighbors(
                source, size_x, size_y, size_z, twist_mode,
                custom_y_shift, custom_z_shift
            ):
                int_links.append(
                    IntLink(
                        link_id=link_id,
                        src_node=routers[source],
                        dst_node=routers[destination],
                        src_outport=outport,
                        dst_inport=inport,
                        latency=options.link_latency,
                        weight=1,
                    )
                )
                link_id += 1
        network.int_links = int_links

    def registerTopology(self, options):
        for index in range(options.num_cpus):
            FileSystemConfig.register_node(
                [index], MemorySize(options.mem_size) // options.num_cpus, index
            )
