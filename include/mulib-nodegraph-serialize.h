/** @file mulib-nodegraph-serialize.h
 * Versioned textual graph topology writer.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No reader or node properties.
 */
#ifndef MULIB_NODEGRAPH_SERIALIZE_H
#define MULIB_NODEGRAPH_SERIALIZE_H
#include "mulib-nodegraph-core.h"
#include "mulib-nodegraph-validate.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
static inline bool mulib_nodegraph_serialize(FILE *f,mulib_nodegraph g){if(!f||!mulib_nodegraph_validate(g)||fprintf(f,"MNG1 %zu %zu\n",g.count,g.edge_count)<0)return false;for(size_t i=0;i<g.count;i++){const mulib_nodegraph_node *n=g.nodes+i;if(fprintf(f,"%llu %u %u %u",(unsigned long long)n->id,(unsigned)n->type,(unsigned)n->output_type,(unsigned)n->input_count)<0)return false;for(uint32_t j=0;j<n->input_count;j++)if(fprintf(f," %u",(unsigned)n->input_types[j])<0)return false;if(fputc('\n',f)==EOF)return false;}for(size_t i=0;i<g.edge_count;i++)if(fprintf(f,"%zu %zu %u\n",g.edges[i].from,g.edges[i].to,(unsigned)g.edges[i].slot)<0)return false;return true;}
#ifdef __cplusplus
}
#endif
#endif
