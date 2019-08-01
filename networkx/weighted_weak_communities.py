from collections import Counter, namedtuple, deque
import math
import networkx as nx
from networkx.utils import union_find
from networkx.utils import not_implemented_for

# -------------------------
# WEIGHTED WEAK COMMUNITIES
# -------------------------

@not_implemented_for('directed')
def weighted_weak_communities(G, min_size=3, dss_iters=2, weight=None):

    dss = dynamic_structural_similarity(G, dss_iters, weight)
    return _weighted_weak_communities(G, dss, min_size)

@not_implemented_for('directed')
def _weighted_weak_communities(G, similarity, min_size=3):

    communities = union_find.UnionFind(G.nodes)
    merged = True

    while merged:
        merged = False
        balance = Counter()
        max_sim = Counter()
        size = Counter()
        new_members = dict()

        for u in G.nodes:
            size[u] = communities.weights[communities[u]]
            new_members[u] = list()

        for u, v in G.edges:
            cu = communities[u]
            cv = communities[v]
            s = similarity[(u, v)]
            if cu == cv:
                balance[cu] += 2*s
            else:
                balance[cu] -= s
                balance[cv] -= s
                _update_candidates(cu, cv, new_members, s, max_sim)
                _update_candidates(cv, cu, new_members, s, max_sim)

        for c in G.nodes:
            if balance[c] < 0.0 or size[c] < min_size:
                for x in new_members[c]:
                    merged |= communities[c] != communities[x]
                    communities.union(c, x)

    return communities.to_sets()

def _update_candidates(u, v, new_members, new_sim, max_sim):

    if max_sim[u] < new_sim:
        max_sim[u] = new_sim
        new_members[u].clear()
    if max_sim[u] == new_sim:
        new_members[u].append(v)

# ----------------------------
# DYNAMIC STRUCTRAL SIMILARITY
# ----------------------------
@not_implemented_for('directed')
def dynamic_structural_similarity(G, iters=2, weight=None):
    
    prev_dss = Counter()
    next_dss = Counter()

    if weight is None:
        real = 1  # this can be any real value != 0. The resulting DSS will be always the same.
        for u, v in G.edges:
            prev_dss[(u, v)] = real
            prev_dss[(v, u)] = real
    else:
        for u, v in G.edges:
            prev_dss[(u, v)] = G[u, v][weight]
            prev_dss[(v, u)] = G[u, v][weight]
    while iters > 0:
        for u, v in G.edges:
            u_nbrs = set(G[u]).union({u})
            v_nbrs = set(G[v]).union({v})

            u_total_dss = sum(prev_dss[(u, x)] for x in u_nbrs)
            v_total_dss = sum(prev_dss[(v, x)] for x in v_nbrs)

            uv_common_dss = sum(prev_dss[(u, x)]+prev_dss[(v, x)] for x in u_nbrs & v_nbrs)

            uv_dss = uv_common_dss/math.sqrt(u_total_dss*v_total_dss)

            next_dss[(u, v)] = uv_dss
            next_dss[(v, u)] = uv_dss

        aux = prev_dss
        prev_dss = next_dss
        next_dss = aux
        iters -= 1
    return prev_dss
