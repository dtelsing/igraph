/* -*- mode: C -*-  */
/* vim:set ts=4 sw=4 sts=4 et: */
/*
   IGraph library.
   Copyright (C) 2005-2021 The igraph development team

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301 USA

*/

#include "igraph_structural.h"

#include "igraph_interface.h"

/**
 * \function igraph_maxdegree
 * \brief The maximum degree in a graph (or set of vertices).
 *
 * The largest in-, out- or total degree of the specified vertices is
 * calculated. If the graph has no vertices, or \p vids is empty,
 * 0 is returned, as this is the smallest possible value for degrees.
 *
 * \param graph The input graph.
 * \param res Pointer to an integer (\c igraph_integer_t), the result
 *        will be stored here.
 * \param vids Vector giving the vertex IDs for which the maximum degree will
 *        be calculated.
 * \param mode Defines the type of the degree.
 *        \c IGRAPH_OUT, out-degree,
 *        \c IGRAPH_IN, in-degree,
 *        \c IGRAPH_ALL, total degree (sum of the
 *        in- and out-degree).
 *        This parameter is ignored for undirected graphs.
 * \param loops Boolean, gives whether the self-loops should be
 *        counted.
 * \return Error code:
 *         \c IGRAPH_EINVVID: invalid vertex ID.
 *         \c IGRAPH_EINVMODE: invalid mode argument.
 *
 * Time complexity: O(v) if \p loops is \c true, and O(v*d) otherwise. v is the number
 * of vertices for which the degree will be calculated, and d is their
 * (average) degree.
 *
 * \sa \ref igraph_degree() to retrieve the degrees for several vertices.
 */
igraph_error_t igraph_maxdegree(const igraph_t *graph, igraph_integer_t *res,
                     igraph_vs_t vids, igraph_neimode_t mode,
                     igraph_bool_t loops) {

    igraph_vector_int_t tmp;

    IGRAPH_VECTOR_INT_INIT_FINALLY(&tmp, 0);

    IGRAPH_CHECK(igraph_degree(graph, &tmp, vids, mode, loops));
    if (igraph_vector_int_size(&tmp) == 0) {
        *res = 0;
    } else {
        *res = igraph_vector_int_max(&tmp);
    }

    igraph_vector_int_destroy(&tmp);
    IGRAPH_FINALLY_CLEAN(1);

    return IGRAPH_SUCCESS;
}

static igraph_error_t igraph_i_avg_nearest_neighbor_degree_weighted(const igraph_t *graph,
        igraph_vs_t vids,
        igraph_neimode_t mode,
        igraph_neimode_t neighbor_degree_mode,
        igraph_vector_t *knn,
        igraph_vector_t *knnk,
        const igraph_vector_t *weights) {

    igraph_integer_t no_of_nodes = igraph_vcount(graph);
    igraph_vector_int_t neis, edge_neis;
    igraph_integer_t i, j, no_vids;
    igraph_vit_t vit;
    igraph_vector_t my_knn_v, *my_knn = knn;
    igraph_vector_t strength;
    igraph_vector_int_t deg;
    igraph_integer_t maxdeg;
    igraph_vector_int_t deghist;

    if (igraph_vector_size(weights) != igraph_ecount(graph)) {
        IGRAPH_ERROR("Invalid weight vector size", IGRAPH_EINVAL);
    }

    IGRAPH_CHECK(igraph_vit_create(graph, vids, &vit));
    IGRAPH_FINALLY(igraph_vit_destroy, &vit);
    no_vids = IGRAPH_VIT_SIZE(vit);

    if (!knn) {
        IGRAPH_VECTOR_INIT_FINALLY(&my_knn_v, no_vids);
        my_knn = &my_knn_v;
    } else {
        IGRAPH_CHECK(igraph_vector_resize(knn, no_vids));
    }

    /* Get degree of neighbours */
    IGRAPH_VECTOR_INT_INIT_FINALLY(&deg, no_of_nodes);
    IGRAPH_CHECK(igraph_degree(graph, &deg, igraph_vss_all(),
                               neighbor_degree_mode, IGRAPH_LOOPS));
    IGRAPH_VECTOR_INIT_FINALLY(&strength, no_of_nodes);

    /* Get strength of all nodes */
    IGRAPH_CHECK(igraph_strength(graph, &strength, igraph_vss_all(),
                                 mode, IGRAPH_LOOPS, weights));

    /* Get maximum degree for initialization */
    IGRAPH_CHECK(igraph_maxdegree(graph, &maxdeg, igraph_vss_all(),
                                  mode, IGRAPH_LOOPS));
    IGRAPH_VECTOR_INT_INIT_FINALLY(&neis, maxdeg);
    IGRAPH_VECTOR_INT_INIT_FINALLY(&edge_neis, maxdeg);
    igraph_vector_int_clear(&neis);
    igraph_vector_int_clear(&edge_neis);

    if (knnk) {
        IGRAPH_CHECK(igraph_vector_resize(knnk, maxdeg));
        igraph_vector_null(knnk);
        IGRAPH_VECTOR_INT_INIT_FINALLY(&deghist, maxdeg);
    }

    for (i = 0; !IGRAPH_VIT_END(vit); IGRAPH_VIT_NEXT(vit), i++) {
        igraph_real_t sum = 0.0;
        igraph_integer_t v = IGRAPH_VIT_GET(vit);
        igraph_integer_t nv;
        igraph_real_t str = VECTOR(strength)[v];
        /* Get neighbours and incident edges */
        IGRAPH_CHECK(igraph_neighbors(graph, &neis, v, mode));
        IGRAPH_CHECK(igraph_incident(graph, &edge_neis, v, mode));
        nv = igraph_vector_int_size(&neis);
        for (j = 0; j < nv; j++) {
            igraph_integer_t nei = VECTOR(neis)[j];
            igraph_integer_t e = VECTOR(edge_neis)[j];
            igraph_real_t w = VECTOR(*weights)[e];
            sum += w * VECTOR(deg)[nei];
        }
        if (str != 0.0) {
            VECTOR(*my_knn)[i] = sum / str;
        } else {
            VECTOR(*my_knn)[i] = IGRAPH_NAN;
        }
        if (knnk && nv > 0) {
            VECTOR(*knnk)[nv - 1] += VECTOR(*my_knn)[i];
            VECTOR(deghist)[nv - 1] += 1;
        }
    }

    igraph_vector_int_destroy(&edge_neis);
    igraph_vector_int_destroy(&neis);
    IGRAPH_FINALLY_CLEAN(2);

    if (knnk) {
        for (i = 0; i < maxdeg; i++) {
            igraph_integer_t dh = VECTOR(deghist)[i];
            if (dh != 0) {
                VECTOR(*knnk)[i] /= dh;
            } else {
                VECTOR(*knnk)[i] = IGRAPH_NAN;
            }
        }

        igraph_vector_int_destroy(&deghist);
        IGRAPH_FINALLY_CLEAN(1);
    }

    igraph_vector_destroy(&strength);
    igraph_vector_int_destroy(&deg);
    IGRAPH_FINALLY_CLEAN(2);

    if (!knn) {
        igraph_vector_destroy(&my_knn_v);
        IGRAPH_FINALLY_CLEAN(1);
    }

    igraph_vit_destroy(&vit);
    IGRAPH_FINALLY_CLEAN(1);

    return IGRAPH_SUCCESS;
}

/**
 * \function igraph_avg_nearest_neighbor_degree
 * \brief Average neighbor degree.
 *
 * Calculates the average degree of the neighbors for each vertex (\p knn), and
 * optionally, the same quantity as a function of the vertex degree (\p knnk).
 *
 * </para><para>
 * For isolated vertices \p knn is set to NaN.
 * The same is done in \p knnk for vertex degrees that
 * don't appear in the graph.
 *
 * </para><para>
 * The weighted version computes a weighted average of the neighbor degrees as
 *
 * </para><para>
 * <code>k_nn_u = 1/s_u sum_v w_uv k_v</code>,
 *
 * </para><para>
 * where <code>s_u = sum_v w_uv</code> is the sum of the incident edge weights
 * of vertex \c u, i.e. its strength.
 * The sum runs over the neighbors \c v of vertex \c u
 * as indicated by \p mode. <code>w_uv</code> denotes the weighted adjacency matrix
 * and <code>k_v</code> is the neighbors' degree, specified by \p neighbor_degree_mode.
 * This is equation (6) in the reference below.
 *
 * </para><para>
 * Reference:
 *
 * </para><para>
 * A. Barrat, M. Barthélemy, R. Pastor-Satorras, and A. Vespignani,
 * The architecture of complex weighted networks,
 * Proc. Natl. Acad. Sci. USA 101, 3747 (2004).
 * https://dx.doi.org/10.1073/pnas.0400087101
 *
 * \param graph The input graph. It may be directed.
 * \param vids The vertices for which the calculation is performed.
 * \param mode The type of neighbors to consider in directed graphs.
 *   \c IGRAPH_OUT considers out-neighbors, \c IGRAPH_IN in-neighbors
 *   and \c IGRAPH_ALL ignores edge directions.
 * \param neighbor_degree_mode The type of degree to average in directed graphs.
 *   \c IGRAPH_OUT averages out-degrees, \c IGRAPH_IN averages in-degrees
 *   and \c IGRAPH_ALL ignores edge directions for the degree calculation.
 * \param vids The vertices for which the calculation is performed.
 * \param knn Pointer to an initialized vector, the result will be
 *   stored here. It will be resized as needed. Supply a \c NULL pointer
 *   here, if you only want to calculate \c knnk.
 * \param knnk Pointer to an initialized vector, the average
 *   neighbor degree as a function of the vertex degree is stored
 *   here. The first (zeroth) element is for degree one vertices,
 *   etc. Supply a \c NULL pointer here if you don't want to calculate
 *   this.
 * \param weights Optional edge weights. Supply a null pointer here
 *   for the non-weighted version.
 *
 * \return Error code.
 *
 * Time complexity: O(|V|+|E|), linear in the number of vertices and
 * edges.
 *
 * \example examples/simple/igraph_knn.c
 */
igraph_error_t igraph_avg_nearest_neighbor_degree(const igraph_t *graph,
                                       igraph_vs_t vids,
                                       igraph_neimode_t mode,
                                       igraph_neimode_t neighbor_degree_mode,
                                       igraph_vector_t *knn,
                                       igraph_vector_t *knnk,
                                       const igraph_vector_t *weights) {

    igraph_integer_t no_of_nodes = igraph_vcount(graph);
    igraph_vector_int_t neis;
    igraph_integer_t no_vids;
    igraph_vit_t vit;
    igraph_vector_t my_knn_v, *my_knn = knn;
    igraph_vector_int_t deg;
    igraph_integer_t maxdeg;
    igraph_vector_int_t deghist;
    igraph_bool_t simple;

    IGRAPH_CHECK(igraph_is_simple(graph, &simple));
    if (!simple) {
        IGRAPH_ERROR("Average nearest neighbor degree works only with "
                     "simple graphs", IGRAPH_EINVAL);
    }

    if (weights) {
        return igraph_i_avg_nearest_neighbor_degree_weighted(graph, vids,
                mode, neighbor_degree_mode, knn, knnk, weights);
    }

    IGRAPH_CHECK(igraph_vit_create(graph, vids, &vit));
    IGRAPH_FINALLY(igraph_vit_destroy, &vit);
    no_vids = IGRAPH_VIT_SIZE(vit);

    if (!knn) {
        IGRAPH_VECTOR_INIT_FINALLY(&my_knn_v, no_vids);
        my_knn = &my_knn_v;
    } else {
        IGRAPH_CHECK(igraph_vector_resize(knn, no_vids));
    }

    IGRAPH_VECTOR_INT_INIT_FINALLY(&deg, no_of_nodes);
    IGRAPH_CHECK(igraph_degree(graph, &deg, igraph_vss_all(),
                               neighbor_degree_mode, IGRAPH_LOOPS));
    IGRAPH_CHECK(igraph_maxdegree(graph, &maxdeg, igraph_vss_all(), mode, IGRAPH_LOOPS));
    IGRAPH_VECTOR_INT_INIT_FINALLY(&neis, maxdeg);
    igraph_vector_int_clear(&neis);

    if (knnk) {
        IGRAPH_CHECK(igraph_vector_resize(knnk, maxdeg));
        igraph_vector_null(knnk);
        IGRAPH_VECTOR_INT_INIT_FINALLY(&deghist, maxdeg);
    }

    for (igraph_integer_t i = 0; !IGRAPH_VIT_END(vit); IGRAPH_VIT_NEXT(vit), i++) {
        igraph_real_t sum = 0.0;
        igraph_integer_t v = IGRAPH_VIT_GET(vit);
        igraph_integer_t nv;
        IGRAPH_CHECK(igraph_neighbors(graph, &neis, v, mode));
        nv = igraph_vector_int_size(&neis);
        for (igraph_integer_t j = 0; j < nv; j++) {
            igraph_integer_t nei = VECTOR(neis)[j];
            sum += VECTOR(deg)[nei];
        }
        if (nv != 0) {
            VECTOR(*my_knn)[i] = sum / nv;
        } else {
            VECTOR(*my_knn)[i] = IGRAPH_NAN;
        }
        if (knnk && nv > 0) {
            VECTOR(*knnk)[nv - 1] += VECTOR(*my_knn)[i];
            VECTOR(deghist)[nv - 1] += 1;
        }
    }

    if (knnk) {
        for (igraph_integer_t i = 0; i < maxdeg; i++) {
            igraph_integer_t dh = VECTOR(deghist)[i];
            if (dh != 0) {
                VECTOR(*knnk)[i] /= dh;
            } else {
                VECTOR(*knnk)[i] = IGRAPH_NAN;
            }
        }
        igraph_vector_int_destroy(&deghist);
        IGRAPH_FINALLY_CLEAN(1);
    }

    igraph_vector_int_destroy(&neis);
    igraph_vector_int_destroy(&deg);
    igraph_vit_destroy(&vit);
    IGRAPH_FINALLY_CLEAN(3);

    if (!knn) {
        igraph_vector_destroy(&my_knn_v);
        IGRAPH_FINALLY_CLEAN(1);
    }

    return IGRAPH_SUCCESS;
}

/**
 * \function igraph_strength
 * \brief Strength of the vertices, also called weighted vertex degree.
 *
 * In a weighted network the strength of a vertex is the sum of the
 * weights of all incident edges. In a non-weighted network this is
 * exactly the vertex degree.
 *
 * \param graph The input graph.
 * \param res Pointer to an initialized vector, the result is stored
 *   here. It will be resized as needed.
 * \param vids The vertices for which the calculation is performed.
 * \param mode Gives whether to count only outgoing (\c IGRAPH_OUT),
 *   incoming (\c IGRAPH_IN) edges or both (\c IGRAPH_ALL).
 * \param loops A logical scalar, whether to count loop edges as well.
 * \param weights A vector giving the edge weights. If this is a \c NULL
 *   pointer, then \ref igraph_degree() is called to perform the
 *   calculation.
 * \return Error code.
 *
 * Time complexity: O(|V|+|E|), linear in the number vertices and
 * edges.
 *
 * \sa \ref igraph_degree() for the traditional, non-weighted version.
 */
igraph_error_t igraph_strength(const igraph_t *graph, igraph_vector_t *res,
                    const igraph_vs_t vids, igraph_neimode_t mode,
                    igraph_bool_t loops, const igraph_vector_t *weights) {

    igraph_integer_t no_of_nodes = igraph_vcount(graph);
    igraph_vit_t vit;
    igraph_integer_t no_vids;
    igraph_vector_int_t degrees;
    igraph_vector_int_t neis;
    igraph_integer_t i;

    if (!weights) {
        IGRAPH_VECTOR_INT_INIT_FINALLY(&degrees, no_of_nodes);
        IGRAPH_CHECK(igraph_vector_resize(res, no_of_nodes));
        IGRAPH_CHECK(igraph_degree(graph, &degrees, vids, mode, loops));
        for (i = 0; i < no_of_nodes; i++) {
            VECTOR(*res)[i] = VECTOR(degrees)[i];
        }
        igraph_vector_int_destroy(&degrees);
        IGRAPH_FINALLY_CLEAN(1);
        return IGRAPH_SUCCESS;
    }


    if (igraph_vector_size(weights) != igraph_ecount(graph)) {
        IGRAPH_ERROR("Invalid weight vector length.", IGRAPH_EINVAL);
    }

    IGRAPH_CHECK(igraph_vit_create(graph, vids, &vit));
    IGRAPH_FINALLY(igraph_vit_destroy, &vit);
    no_vids = IGRAPH_VIT_SIZE(vit);

    IGRAPH_VECTOR_INT_INIT_FINALLY(&neis, 0);
    IGRAPH_CHECK(igraph_vector_int_reserve(&neis, no_of_nodes));
    IGRAPH_CHECK(igraph_vector_resize(res, no_vids));
    igraph_vector_null(res);

    if (loops) {
        for (i = 0; !IGRAPH_VIT_END(vit); IGRAPH_VIT_NEXT(vit), i++) {
            igraph_integer_t vid = IGRAPH_VIT_GET(vit);
            igraph_integer_t j, n;
            IGRAPH_CHECK(igraph_incident(graph, &neis, vid, mode));
            n = igraph_vector_int_size(&neis);
            for (j = 0; j < n; j++) {
                igraph_integer_t edge = VECTOR(neis)[j];
                VECTOR(*res)[i] += VECTOR(*weights)[edge];
            }
        }
    } else {
        for (i = 0; !IGRAPH_VIT_END(vit); IGRAPH_VIT_NEXT(vit), i++) {
            igraph_integer_t vid = IGRAPH_VIT_GET(vit);
            igraph_integer_t j, n;
            IGRAPH_CHECK(igraph_incident(graph, &neis, vid, mode));
            n = igraph_vector_int_size(&neis);
            for (j = 0; j < n; j++) {
                igraph_integer_t edge = VECTOR(neis)[j];
                igraph_integer_t from = IGRAPH_FROM(graph, edge);
                igraph_integer_t to = IGRAPH_TO(graph, edge);
                if (from != to) {
                    VECTOR(*res)[i] += VECTOR(*weights)[edge];
                }
            }
        }
    }

    igraph_vit_destroy(&vit);
    igraph_vector_int_destroy(&neis);
    IGRAPH_FINALLY_CLEAN(2);

    return IGRAPH_SUCCESS;
}


/**
 * \function igraph_sort_vertex_ids_by_degree
 * \brief Calculate a list of vertex IDs sorted by degree of the corresponding vertex.
 *
 * The list of vertex IDs is returned in a vector that is sorted
 * in ascending or descending order of vertex degree.
 *
 * \param graph The input graph.
 * \param outvids Pointer to an initialized vector that will be
 *        resized and will contain the ordered vertex IDs.
 * \param vids Input vertex selector of vertex IDs to include in
 *        calculation.
 * \param mode Defines the type of the degree.
 *        \c IGRAPH_OUT, out-degree,
 *        \c IGRAPH_IN, in-degree,
 *        \c IGRAPH_ALL, total degree (sum of the
 *        in- and out-degree).
 *        This parameter is ignored for undirected graphs.
 * \param loops Boolean, gives whether the self-loops should be
 *        counted.
 * \param order Specifies whether the ordering should be ascending
 *        (\c IGRAPH_ASCENDING) or descending (\c IGRAPH_DESCENDING).
 * \param only_indices If true, then return a sorted list of indices
 *        into a vector corresponding to \c vids, rather than a list
 *        of vertex IDs. This parameter is ignored if \c vids is set
 *        to all vertices via \ref igraph_vs_all() or \ref igraph_vss_all(),
 *        because in this case the indices and vertex IDs are the
 *        same.
 * \return Error code:
 *         \c IGRAPH_EINVVID: invalid vertex ID.
 *         \c IGRAPH_EINVMODE: invalid mode argument.
 *
 */
igraph_error_t igraph_sort_vertex_ids_by_degree(const igraph_t *graph,
                                     igraph_vector_int_t *outvids,
                                     igraph_vs_t vids,
                                     igraph_neimode_t mode,
                                     igraph_bool_t loops,
                                     igraph_order_t order,
                                     igraph_bool_t only_indices) {
    igraph_integer_t i, n;
    igraph_vector_int_t degrees;
    igraph_vector_int_t vs_vec;
    IGRAPH_VECTOR_INT_INIT_FINALLY(&degrees, 0);
    IGRAPH_CHECK(igraph_degree(graph, &degrees, vids, mode, loops));
    IGRAPH_CHECK(igraph_vector_int_qsort_ind(&degrees, outvids, order));
    if (only_indices || igraph_vs_is_all(&vids) ) {
        igraph_vector_int_destroy(&degrees);
        IGRAPH_FINALLY_CLEAN(1);
    } else {
        IGRAPH_VECTOR_INT_INIT_FINALLY(&vs_vec, 0);
        IGRAPH_CHECK(igraph_vs_as_vector(graph, vids, &vs_vec));
        n = igraph_vector_int_size(outvids);
        for (i = 0; i < n; i++) {
            VECTOR(*outvids)[i] = VECTOR(vs_vec)[VECTOR(*outvids)[i]];
        }
        igraph_vector_int_destroy(&vs_vec);
        igraph_vector_int_destroy(&degrees);
        IGRAPH_FINALLY_CLEAN(2);
    }
    return IGRAPH_SUCCESS;
}
