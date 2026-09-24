/*
 * Copyright (c) 2008 Princeton University
 * Copyright (c) 2016 Georgia Institute of Technology
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "mem/ruby/network/garnet/RoutingUnit.hh"

#include <queue>

#include "base/cast.hh"
#include "base/compiler.hh"
#include "debug/RubyNetwork.hh"
#include "mem/ruby/network/garnet/InputUnit.hh"
#include "mem/ruby/network/garnet/OutputUnit.hh"
#include "mem/ruby/network/garnet/Router.hh"
#include "mem/ruby/slicc_interface/Message.hh"

namespace gem5
{

namespace ruby
{

namespace garnet
{

RoutingUnit::RoutingUnit(Router *router)
{
    m_router = router;
    m_routing_table.clear();
    m_weight_table.clear();
}

void
RoutingUnit::addRoute(std::vector<NetDest>& routing_table_entry)
{
    if (routing_table_entry.size() > m_routing_table.size()) {
        m_routing_table.resize(routing_table_entry.size());
    }
    for (int v = 0; v < routing_table_entry.size(); v++) {
        m_routing_table[v].push_back(routing_table_entry[v]);
    }
}

void
RoutingUnit::addWeight(int link_weight)
{
    m_weight_table.push_back(link_weight);
}

bool
RoutingUnit::supportsVnet(int vnet, std::vector<int> sVnets)
{
    // If all vnets are supported, return true
    if (sVnets.size() == 0) {
        return true;
    }

    // Find the vnet in the vector, return true
    if (std::find(sVnets.begin(), sVnets.end(), vnet) != sVnets.end()) {
        return true;
    }

    // Not supported vnet
    return false;
}

/*
 * This is the default routing algorithm in garnet.
 * The routing table is populated during topology creation.
 * Routes can be biased via weight assignments in the topology file.
 * Correct weight assignments are critical to provide deadlock avoidance.
 */
int
RoutingUnit::lookupRoutingTable(int vnet, NetDest msg_destination)
{
    // First find all possible output link candidates
    // For ordered vnet, just choose the first
    // (to make sure different packets don't choose different routes)
    // For unordered vnet, randomly choose any of the links
    // To have a strict ordering between links, they should be given
    // different weights in the topology file

    int output_link = -1;
    int min_weight = INFINITE_;
    std::vector<int> output_link_candidates;
    int num_candidates = 0;

    // Identify the minimum weight among the candidate output links
    for (int link = 0; link < m_routing_table[vnet].size(); link++) {
        if (msg_destination.intersectionIsNotEmpty(
            m_routing_table[vnet][link])) {

        if (m_weight_table[link] <= min_weight)
            min_weight = m_weight_table[link];
        }
    }

    // Collect all candidate output links with this minimum weight
    for (int link = 0; link < m_routing_table[vnet].size(); link++) {
        if (msg_destination.intersectionIsNotEmpty(
            m_routing_table[vnet][link])) {

            if (m_weight_table[link] == min_weight) {
                num_candidates++;
                output_link_candidates.push_back(link);
            }
        }
    }

    if (output_link_candidates.size() == 0) {
        fatal("Fatal Error:: No Route exists from this Router.");
        exit(0);
    }

    // Randomly select any candidate output link
    int candidate = 0;
    if (!(m_router->get_net_ptr())->isVNetOrdered(vnet))
        candidate = rand() % num_candidates;

    output_link = output_link_candidates.at(candidate);
    return output_link;
}


void
RoutingUnit::addInDirection(PortDirection inport_dirn, int inport_idx)
{
    m_inports_dirn2idx[inport_dirn] = inport_idx;
    m_inports_idx2dirn[inport_idx]  = inport_dirn;
}

void
RoutingUnit::addOutDirection(PortDirection outport_dirn, int outport_idx)
{
    m_outports_dirn2idx[outport_dirn] = outport_idx;
    m_outports_idx2dirn[outport_idx]  = outport_dirn;
}

// outportCompute() is called by the InputUnit
// It calls the routing table by default.
// A template for adaptive topology-specific routing algorithm
// implementations using port directions rather than a static routing
// table is provided here.

int
RoutingUnit::outportCompute(RouteInfo route, int inport,
                            PortDirection inport_dirn)
{
    int outport = -1;

    if (route.dest_router == m_router->get_id()) {

        // Multiple NIs may be connected to this router,
        // all with output port direction = "Local"
        // Get exact outport id from table
        outport = lookupRoutingTable(route.vnet, route.net_dest);
        return outport;
    }

    // Routing Algorithm set in GarnetNetwork.py
    // Can be over-ridden from command line using --routing-algorithm = 1
    RoutingAlgorithm routing_algorithm =
        (RoutingAlgorithm) m_router->get_net_ptr()->getRoutingAlgorithm();

    switch (routing_algorithm) {
        case TABLE_:  outport =
            lookupRoutingTable(route.vnet, route.net_dest); break;
        case XY_:     outport =
            outportComputeXY(route, inport, inport_dirn); break;
        // any custom algorithm
        case CUSTOM_: outport =
            outportComputeCustom(route, inport, inport_dirn); break;
        default: outport =
            lookupRoutingTable(route.vnet, route.net_dest); break;
    }

    assert(outport != -1);
    return outport;
}

// XY routing implemented using port directions
// Only for reference purpose in a Mesh
// By default Garnet uses the routing table
int
RoutingUnit::outportComputeXY(RouteInfo route,
                              int inport,
                              PortDirection inport_dirn)
{
    PortDirection outport_dirn = "Unknown";

    [[maybe_unused]] int num_rows = m_router->get_net_ptr()->getNumRows();
    int num_cols = m_router->get_net_ptr()->getNumCols();
    assert(num_rows > 0 && num_cols > 0);

    int my_id = m_router->get_id();
    int my_x = my_id % num_cols;
    int my_y = my_id / num_cols;

    int dest_id = route.dest_router;
    int dest_x = dest_id % num_cols;
    int dest_y = dest_id / num_cols;

    int x_hops = abs(dest_x - my_x);
    int y_hops = abs(dest_y - my_y);

    bool x_dirn = (dest_x >= my_x);
    bool y_dirn = (dest_y >= my_y);

    // already checked that in outportCompute() function
    assert(!(x_hops == 0 && y_hops == 0));

    if (x_hops > 0) {
        if (x_dirn) {
            assert(inport_dirn == "Local" || inport_dirn == "West");
            outport_dirn = "East";
        } else {
            assert(inport_dirn == "Local" || inport_dirn == "East");
            outport_dirn = "West";
        }
    } else if (y_hops > 0) {
        if (y_dirn) {
            // "Local" or "South" or "West" or "East"
            assert(inport_dirn != "North");
            outport_dirn = "North";
        } else {
            // "Local" or "North" or "West" or "East"
            assert(inport_dirn != "South");
            outport_dirn = "South";
        }
    } else {
        // x_hops == 0 and y_hops == 0
        // this is not possible
        // already checked that in outportCompute() function
        panic("x_hops == y_hops == 0");
    }

    return m_outports_dirn2idx[outport_dirn];
}

// Template for implementing custom routing algorithm
// using port directions. (Example adaptive)
int
RoutingUnit::outportComputeCustom(RouteInfo route,
                                 int inport,
                                 PortDirection inport_dirn)
{
    const int num_routers = m_router->get_net_ptr()->getNumRouters();
    if (num_routers == 16 && m_outports_dirn2idx.count("Clockwise")) {
        const int current = m_router->get_id();
        const int destination = route.dest_router;
        const int clockwise_hops =
            (destination - current + num_routers) % num_routers;
        const int counter_clockwise_hops =
            (current - destination + num_routers) % num_routers;

        if (clockwise_hops <= counter_clockwise_hops)
            return m_outports_dirn2idx["Clockwise"];
        return m_outports_dirn2idx["CounterClockwise"];
    }

    const auto candidates = twistedMinimalOutports(route.dest_router);
    fatal_if(candidates.empty(),
        "TwistRoute found no minimal output from router %d to %d",
        m_router->get_id(), route.dest_router);

    const std::string &policy =
        m_router->get_net_ptr()->getTwistRoutingPolicy();
    if (policy == "escape")
        return escapeOutport(route.dest_router);
    if (policy == "flex")
        return flexOutport(route, inport_dirn);
    if (policy == "deterministic")
        return candidates.front();
    if (policy == "random")
        return candidates[rand() % candidates.size()];
    fatal_if(policy != "adaptive", "Unknown TwistRoute policy %s", policy);
    return congestionAwareOutport(candidates, route.vnet);
}

int
RoutingUnit::congestionAwareOutport(const std::vector<int>& candidates,
                                    int vnet) const
{
    const int vcs_per_vnet = m_router->get_vc_per_vnet();
    fatal_if(vcs_per_vnet < 2,
             "TwistRoute requires at least two VCs per vnet");
    const int first_vc = vnet * vcs_per_vnet;
    int best_credit_score = -1;
    std::vector<int> best;

    // Credits are exact downstream free-buffer slots. Summing them across the
    // vnet's VCs favors the output with the most immediately usable capacity.
    for (const int outport : candidates) {
        auto *output = m_router->getOutputUnit(outport);
        int credit_score = 0;
        for (int offset = 0; offset < vcs_per_vnet - 1; ++offset)
            credit_score += output->get_credit_count(first_vc + offset);

        if (credit_score > best_credit_score) {
            best_credit_score = credit_score;
            best.clear();
            best.push_back(outport);
        } else if (credit_score == best_credit_score) {
            best.push_back(outport);
        }
    }

    assert(!best.empty());
    return best[rand() % best.size()];
}

int
RoutingUnit::flexOutport(RouteInfo route, PortDirection inport_dirn) const
{
    static const std::vector<PortDirection> directions = {
        "East", "West", "North", "South", "Up", "Down"
    };
    struct Candidate {
        int outport;
        int credit;
        int lookahead_credit;
        int diversity;
        bool free_vc;
    };

    const auto &distance = twistedDistances(route.dest_router);
    const int current = m_router->get_id();
    const int vcs_per_vnet = m_router->get_vc_per_vnet();
    const int first_vc = route.vnet * vcs_per_vnet;
    const int adaptive_vcs = vcs_per_vnet - 1;
    fatal_if(adaptive_vcs < 1, "FlexRoute requires an adaptive VC");

    auto make_candidate = [&](PortDirection direction, int neighbor) {
        const int outport = m_outports_dirn2idx.at(direction);
        auto *output = m_router->getOutputUnit(outport);
        int credits = 0;
        for (int offset = 0; offset < adaptive_vcs; ++offset)
            credits += output->get_credit_count(first_vc + offset);
        int diversity = 0;
        int lookahead_credit = -1;
        auto *neighbor_router =
            m_router->get_net_ptr()->getRouterById(neighbor);
        for (const auto &next_direction : directions) {
            const int next = twistedNeighbor(neighbor, next_direction);
            if (distance[next] == distance[neighbor] - 1) {
                ++diversity;
                lookahead_credit = std::max(
                    lookahead_credit,
                    neighbor_router->adaptive_credit(next_direction,
                                                     route.vnet));
            }
        }
        if (neighbor == route.dest_router)
            lookahead_credit = credits;
        fatal_if(lookahead_credit < 0,
                 "FlexRoute found no onward minimal output at router %d",
                 neighbor);
        return Candidate{outport, credits, lookahead_credit, diversity,
                         output->has_free_vc(route.vnet, false)};
    };

    std::vector<Candidate> minimal;
    std::vector<Candidate> lateral;
    for (const auto &direction : directions) {
        const int neighbor = twistedNeighbor(current, direction);
        if (distance[neighbor] == distance[current] - 1) {
            minimal.push_back(make_candidate(direction, neighbor));
        } else if (direction != inport_dirn &&
                   distance[neighbor] == distance[current]) {
            lateral.push_back(make_candidate(direction, neighbor));
        }
    }
    fatal_if(minimal.empty(),
             "FlexRoute found no minimal output from router %d to %d",
             current, route.dest_router);

    const int source_distance = twistedDistances(route.dest_router)
                                    [route.src_router];
    fatal_if(route.hops_traversed + distance[current] > source_distance + 1,
             "FlexRoute exceeded its one-hop slack budget from %d to %d",
             route.src_router, route.dest_router);
    const bool slack_available =
        route.hops_traversed + distance[current] == source_distance;

    int best_min_path_credit = -1;
    bool any_min_free = false;
    for (const auto &candidate : minimal) {
        best_min_path_credit = std::max(
            best_min_path_credit,
            std::min(candidate.credit, candidate.lookahead_credit));
        any_min_free = any_min_free || candidate.free_vc;
    }

    auto *net = m_router->get_net_ptr();
    const int buffers_per_vc =
        net->get_vnet_type(route.vnet) == CTRL_VNET_
            ? net->getBuffersPerCtrlVC()
            : net->getBuffersPerDataVC();
    const int credit_capacity = adaptive_vcs * buffers_per_vc;
    const bool credit_pressure =
        100 * best_min_path_credit <
        net->getFlexCreditThresholdPct() * credit_capacity;

    std::vector<Candidate> eligible_lateral;
    if (slack_available && (!any_min_free || credit_pressure)) {
        for (const auto &candidate : lateral) {
            const bool useful =
                !any_min_free ||
                std::min(candidate.credit, candidate.lookahead_credit) >=
                    best_min_path_credit + net->getFlexCreditMargin();
            if (candidate.free_vc && useful)
                eligible_lateral.push_back(candidate);
        }
    }

    if (eligible_lateral.empty()) {
        std::vector<int> minimal_outports;
        minimal_outports.reserve(minimal.size());
        for (const auto &candidate : minimal)
            minimal_outports.push_back(candidate.outport);
        return congestionAwareOutport(minimal_outports, route.vnet);
    }

    const auto &pool = eligible_lateral;
    const int diversity_weight = net->getFlexDiversityWeight();
    int best_score = -1;
    for (const auto &candidate : pool) {
        const int score = std::min(candidate.credit,
                                   candidate.lookahead_credit) +
                          diversity_weight * candidate.diversity;
        best_score = std::max(best_score, score);
    }

    std::vector<std::pair<int, int>> randomized;
    int total_weight = 0;
    for (const auto &candidate : pool) {
        const int score = std::min(candidate.credit,
                                   candidate.lookahead_credit) +
                          diversity_weight * candidate.diversity;
        if (score + net->getFlexScoreEpsilon() >= best_score) {
            const int weight = std::max(1, score + 1);
            randomized.emplace_back(candidate.outport, weight);
            total_weight += weight;
        }
    }
    assert(!randomized.empty() && total_weight > 0);
    int draw = rand() % total_weight;
    int selected = randomized.back().first;
    for (const auto &[outport, weight] : randomized) {
        if (draw < weight) {
            selected = outport;
            break;
        }
        draw -= weight;
    }
    net->incrementFlexLateralSelections();
    return selected;
}

int
RoutingUnit::twistedNeighbor(int id, PortDirection direction) const
{
    const auto *net = m_router->get_net_ptr();
    const int sx = net->getTorusX();
    const int sy = net->getTorusY();
    const int sz = net->getTorusZ();
    const int plane = sx * sy;
    int z = id / plane;
    int offset = id % plane;
    int y = offset / sx;
    int x = offset % sx;
    const std::string &mode = net->getTwistMode();
    const int y_shift = mode == "custom" ? net->getTwistYShift() % sx :
        (mode == "synth" ? sy - 1 : sy);
    const int z_shift = mode == "custom" ? net->getTwistZShift() % sx : sz;

    if (direction == "East") x = (x + 1) % sx;
    else if (direction == "West") x = (x + sx - 1) % sx;
    else if (direction == "North") {
        if (y == sy - 1 && mode != "none") x = (x + y_shift) % sx;
        y = (y + 1) % sy;
    } else if (direction == "South") {
        if (y == 0 && mode != "none") x = (x + sx - y_shift) % sx;
        y = (y + sy - 1) % sy;
    } else if (direction == "Up") {
        if (z == sz - 1 &&
            (mode == "double" || mode == "synth" || mode == "custom"))
            x = (x + z_shift) % sx;
        z = (z + 1) % sz;
    } else if (direction == "Down") {
        if (z == 0 &&
            (mode == "double" || mode == "synth" || mode == "custom"))
            x = (x + sx - z_shift) % sx;
        z = (z + sz - 1) % sz;
    } else {
        panic("Invalid TwistedTorus direction %s", direction);
    }
    return (z * sy + y) * sx + x;
}

std::vector<int>
RoutingUnit::twistedMinimalOutports(int destination) const
{
    static const std::vector<PortDirection> directions = {
        "East", "West", "North", "South", "Up", "Down"
    };
    const auto &distance = twistedDistances(destination);

    const int current = m_router->get_id();
    std::vector<int> candidates;
    for (const auto &direction : directions) {
        const int neighbor = twistedNeighbor(current, direction);
        if (distance[neighbor] == distance[current] - 1) {
            auto port = m_outports_dirn2idx.find(direction);
            fatal_if(port == m_outports_dirn2idx.end(),
                "TwistRoute router %d has no %s output", current, direction);
            candidates.push_back(port->second);
        }
    }
    return candidates;
}

const std::vector<int>&
RoutingUnit::twistedDistances(int destination) const
{
    static const std::vector<PortDirection> directions = {
        "East", "West", "North", "South", "Up", "Down"
    };
    const int count = m_router->get_net_ptr()->getNumRouters();
    fatal_if(count != m_router->get_net_ptr()->getTorusX() *
                      m_router->get_net_ptr()->getTorusY() *
                      m_router->get_net_ptr()->getTorusZ(),
             "TwistRoute dimensions do not match router count");

    // The immutable topology is shared by all RoutingUnits in one gem5 run.
    static std::vector<std::vector<int>> distance_cache;
    if (distance_cache.size() != count)
        distance_cache.assign(count, {});
    auto &distance = distance_cache[destination];
    if (distance.empty()) {
        distance.assign(count, -1);
        std::queue<int> frontier;
        distance[destination] = 0;
        frontier.push(destination);
        while (!frontier.empty()) {
            const int node = frontier.front();
            frontier.pop();
            for (const auto &direction : directions) {
                const int neighbor = twistedNeighbor(node, direction);
                if (distance[neighbor] == -1) {
                    distance[neighbor] = distance[node] + 1;
                    frontier.push(neighbor);
                }
            }
        }
    }
    return distance;
}

int
RoutingUnit::escapeOutport(int destination) const
{
    static const std::vector<PortDirection> directions = {
        "East", "West", "North", "South", "Up", "Down"
    };
    const int count = m_router->get_net_ptr()->getNumRouters();
    const int current = m_router->get_id();

    // Deterministic BFS tree rooted at router 0.  Parent links are fixed by
    // the direction order above and are independent of traffic state.
    static std::vector<int> escape_parent_cache;
    if (escape_parent_cache.size() != count) {
        escape_parent_cache.assign(count, -1);
        std::queue<int> frontier;
        escape_parent_cache[0] = 0;
        frontier.push(0);
        while (!frontier.empty()) {
            const int node = frontier.front();
            frontier.pop();
            for (const auto &direction : directions) {
                const int neighbor = twistedNeighbor(node, direction);
                if (escape_parent_cache[neighbor] == -1) {
                    escape_parent_cache[neighbor] = node;
                    frontier.push(neighbor);
                }
            }
        }
    }
    const auto &parent = escape_parent_cache;
    fatal_if(parent[destination] == -1,
             "Escape tree does not reach router %d", destination);

    // If current is an ancestor of destination, descend to the child on the
    // unique tree path. Otherwise move upward to current's parent.
    int next = destination;
    while (next != 0 && parent[next] != current)
        next = parent[next];
    if (parent[next] != current)
        next = parent[current];

    fatal_if(next < 0 || next == current,
             "Invalid escape step from router %d to %d", current,
             destination);
    for (const auto &direction : directions) {
        if (twistedNeighbor(current, direction) == next) {
            auto port = m_outports_dirn2idx.find(direction);
            fatal_if(port == m_outports_dirn2idx.end(),
                     "Escape output %s missing at router %d", direction,
                     current);
            return port->second;
        }
    }
    panic("Escape tree edge %d -> %d is not a physical link", current, next);
}

} // namespace garnet
} // namespace ruby
} // namespace gem5
