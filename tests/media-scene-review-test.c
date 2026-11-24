/* New scene kernels only; no production/foreign ABI admission implied.
 * cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined \
 *   -Ikernels tests/media-scene-review-test.c -lm -o /tmp/opencode/scene-test
 */
#include "mulib-mesh-primitives.h"
#include "mulib-mesh-subdivide.h"
#include "mulib-mesh-triangulate.h"
#include "mulib-mesh-remesh.h"
#include "mulib-mesh-attributes.h"
#include "mulib-mesh-uv.h"
#include "mulib-geom-bezier.h"
#include "mulib-geom-nurbs.h"
#include "mulib-geom-isosurface.h"
#include "mulib-geom-cloner.h"
#include "mulib-anim-ik.h"
#include "mulib-anim-bake.h"
#include "mulib-anim-driver.h"
#include "mulib-anim-morph.h"
#include "mulib-anim-quaternion.h"
#include "mulib-physics-solver.h"
#include "mulib-physics-joint.h"
#include "mulib-physics-world.h"
#include "mulib-render-bvhbuild.h"
#include "mulib-render-film.h"
#include <assert.h>

static uint64_t scene_rng = UINT64_C(0x39b54ff32107a615);
static uint32_t scene_random(void)
{
    scene_rng ^= scene_rng << 13;
    scene_rng ^= scene_rng >> 7;
    scene_rng ^= scene_rng << 17;
    return (uint32_t)scene_rng;
}

static void test_mesh(void)
{
    mulib_mesh_vec3 v[8], dst[32], scratch[8];
    mulib_mesh_tri t[12], dt[48];
    mulib_mesh m = {v, 0, 8, t, 0, 12};
    mulib_mesh d = {dst, 0, 32, dt, 0, 48};
    mulib_mesh_halfedge edges[36], saved_edges[36];
    uint32_t ids[36];
    assert(mulib_mesh_primitives_box(&m, mulib_mesh_v(1, 1, 1)));
    assert(!mulib_mesh_subdivide(&m, &d, edges, 36, ids, 35));
    assert(d.vertex_count == 0 && d.triangle_count == 0);
    assert(mulib_mesh_subdivide(&m, &d, edges, 36, ids, 36));
    assert(d.vertex_count == 26 && d.triangle_count == 48);
    assert(mulib_mesh_valid(&d));
    d = (mulib_mesh){v, 0, 8, dt, 0, 48};
    assert(!mulib_mesh_subdivide(&m, &d, edges, 36, ids, 36));
    memset(edges, 0xa5, sizeof edges);
    memcpy(saved_edges, edges, sizeof edges);
    mulib_mesh_tri old = t[1];
    t[1] = t[0];
    assert(!mulib_mesh_halfedge_build(&m, edges, 36));
    assert(memcmp(saved_edges, edges, sizeof edges) == 0);
    t[1] = old;
    mulib_mesh_vec3 original[8];
    memcpy(original, v, sizeof v);
    assert(!mulib_mesh_remesh_smooth(&m, scratch, 7, NULL, 0, .5));
    assert(!mulib_mesh_remesh_smooth(&m, v + 1, 8, NULL, 0, .5));
    bool fixed[8] = {true};
    assert(!mulib_mesh_remesh_smooth(&m, scratch, 8, fixed, 7, .5));
    assert(memcmp(original, v, sizeof v) == 0);
    assert(mulib_mesh_remesh_smooth(&m, scratch, 8, fixed, 8, .5));
    assert(v[0].x == original[0].x);
    mulib_mesh huge = m;
    huge.triangle_capacity = SIZE_MAX;
    assert(!mulib_mesh_valid(&huge));
    assert(!mulib_mesh_append_tri(&huge, 0, 1, 2));
    float a = 1;
    assert(!mulib_mesh_attribute_valid(
        (mulib_mesh_attribute){&a, 1, 1, SIZE_MAX}));
    mulib_mesh_vec3 extremes[2] = {{-DBL_MAX, 0, 0}, {DBL_MAX, 1, 0}};
    mulib_mesh uvmesh = {extremes, 2, 2, NULL, 0, 0};
    double uv[4];
    assert(mulib_mesh_uv_planar(&uvmesh, uv, 4, 2));
    assert(uv[0] == 0 && uv[2] == 1);
    mulib_mesh_vec3 u = mulib_mesh_unit(mulib_mesh_v(DBL_MAX, DBL_MAX, 0));
    assert(fabs(u.x - sqrt(.5)) < 1e-15);
    u = mulib_mesh_unit(mulib_mesh_v(DBL_TRUE_MIN, 0, 0));
    assert(u.x == 1 && u.y == 0 && u.z == 0);
}

static void test_triangulate(void)
{
    const mulib_mesh_pt2 concave[5] = {{0,0},{2,0},{2,2},{1,1},{0,2}};
    uint32_t scratch[5];
    mulib_mesh_tri out[3], saved[3];
    memset(out, 0x7c, sizeof out);
    memcpy(saved, out, sizeof out);
    assert(!mulib_mesh_triangulate(concave, 5, scratch, 4, out, 3));
    assert(memcmp(saved, out, sizeof out) == 0);
    assert(mulib_mesh_triangulate(concave, 5, scratch, 5, out, 3));
    for(size_t i=0;i<3;i++)
        assert(out[i].a<5 && out[i].b<5 && out[i].c<5);
    mulib_mesh_pt2 crossing[4] = {{0,0},{3,2},{0,2},{2,0}};
    memcpy(saved, out, sizeof out);
    assert(!mulib_mesh_triangulate(crossing, 4, scratch, 5, out, 3));
    assert(memcmp(saved, out, sizeof out) == 0);
    mulib_mesh_pt2 bad[3] = {{0,0},{1,0},{0,NAN}};
    assert(!mulib_mesh_triangulate(bad, 3, scratch, 5, out, 3));
    assert(!mulib_mesh_triangulate(concave, SIZE_MAX, scratch, 5, out, 3));
    /* Independent capacity and area oracle over randomized convex quadrilaterals. */
    for(unsigned k=0;k<1000;k++){
        double w=1+(scene_random()%1000)/100.0;
        double h=1+(scene_random()%1000)/100.0;
        mulib_mesh_pt2 p[4]={{0,0},{w,0},{w,h},{0,h}};
        size_t cap=scene_random()%6;
        memcpy(saved,out,sizeof out);
        bool ok=mulib_mesh_triangulate(p,4,scratch,cap,out,3);
        assert(ok==(cap>=4));
        if(!ok)assert(memcmp(saved,out,sizeof out)==0);
        else{
            double area=0;
            for(size_t i=0;i<2;i++){
                assert(out[i].a<4&&out[i].b<4&&out[i].c<4);
                area+=mulib_mesh_tri_cross2(p[out[i].a],p[out[i].b],p[out[i].c])*.5;
            }
            assert(fabs(area-w*h)<1e-10);
        }
    }
}

static void test_geometry(void)
{
    const mulib_mesh_vec3 control[2]={{0,0,0},{2,0,0}};
    mulib_mesh_vec3 scratch[2], out={9,8,7}, saved=out;
    const double knots[4]={0,0,1,1}, weights[2]={1,1};
    assert(!mulib_geom_bezier_eval(control,2,.5,scratch,1,&out));
    assert(out.x==saved.x);
    assert(mulib_geom_bezier_eval(control,2,.5,scratch,2,&out)&&out.x==1);
    out=saved;
    assert(!mulib_geom_bspline_eval(control,2,knots,3,1,.5,&out));
    assert(out.x==saved.x);
    assert(!mulib_geom_nurbs_eval(control,weights,1,2,knots,4,1,.5,&out));
    assert(mulib_geom_bspline_eval(control,2,knots,4,1,.5,&out)&&out.x==1);
    assert(mulib_geom_nurbs_eval(control,weights,2,2,knots,4,1,.5,&out)&&out.x==1);
    const mulib_mesh_vec3 repeated[3]={{0,0,0},{2,0,0},{99,0,0}};
    const double end_knots[5]={0,0,1,1,1};
    assert(mulib_geom_bspline_eval(repeated,3,end_knots,5,1,1,&out));
    assert(out.x==2); /* Last nonzero span, not the zero-width final span. */
    const double wide[4]={-DBL_MAX,-DBL_MAX,DBL_MAX,DBL_MAX};
    out=saved;
    assert(!mulib_geom_bspline_eval(control,2,wide,4,1,0,&out));
    assert(out.x==saved.x);
    mulib_mesh_vec3 tet[4]={{0,0,0},{1,0,0},{0,1,0},{0,0,1}}, iso[6];
    double field[4]={-DBL_MAX,-DBL_MAX,DBL_MAX,DBL_MAX};
    memset(iso,0x45,sizeof iso);
    mulib_mesh_vec3 copy[6];memcpy(copy,iso,sizeof iso);
    assert(mulib_geom_isosurface_tet(tet,field,0,iso,5)==0);
    assert(memcmp(copy,iso,sizeof iso)==0);
    assert(mulib_geom_isosurface_tet(tet,field,0,iso,6)==2);
    for(size_t i=0;i<6;i++)assert(mulib_mesh_finite(iso[i]));
    mulib_mesh_vec3 clones[3]={{7,8,9},{7,8,9},{7,8,9}};
    assert(!mulib_geom_cloner_grid(clones,3,3,1,1,mulib_mesh_v(DBL_MAX,0,0)));
    assert(clones[0].x==7);
    for(unsigned i=0;i<1000;i++){
        size_t kc=scene_random()%5;
        out=saved;
        bool ok=mulib_geom_bspline_eval(control,2,knots,kc,1,.25,&out);
        assert(ok==(kc>=4));
        assert(out.x==(ok?.5:saved.x));
    }
}

static bool driver(void *ctx,size_t i,const double *deps,size_t n,double *out)
{
    unsigned *calls=ctx;
    (*calls)++;
    *out=(double)i+1;
    for(size_t j=0;j<n;j++)*out+=deps[j];
    return true;
}

static void test_animation(void)
{
    mulib_anim_key keys[2]={{INT64_MIN,0,0,0},{INT64_MAX,2,0,0}};
    double value=99;
    assert(mulib_anim_curve_eval(keys,2,0,MULIB_ANIM_LINEAR,&value));
    assert(fabs(value-1)<1e-15);
    keys[0].tick=INT64_MAX-2;
    assert(mulib_anim_curve_eval(keys,2,INT64_MAX-1,MULIB_ANIM_LINEAR,&value));
    assert(value==1);
    mulib_anim_channel channels[2]={{keys,2,MULIB_ANIM_LINEAR},{NULL,0,MULIB_ANIM_LINEAR}};
    double out[2]={42,43};
    assert(!mulib_anim_channel_eval(channels,2,0,out));
    assert(out[0]==42&&out[1]==43);
    assert(!mulib_anim_bake(channels,INT64_MAX,1,out,2));
    assert(out[0]==42&&out[1]==43);
    mulib_mesh_vec3 p[2]={{0,0,0},{1,0,0}};
    double len[1]={1};
    assert(!mulib_anim_ik_fabrik(p,len,0,2,mulib_mesh_v(1,0,0),5,1e-6));
    assert(mulib_anim_ik_fabrik(p,len,1,2,mulib_mesh_v(1,0,0),5,1e-6));
    size_t offsets[3]={0,0,1},deps[1]={0};
    double scratch[1];
    unsigned calls=0;
    assert(!mulib_anim_driver_eval(offsets,2,deps,1,2,out,scratch,1,driver,&calls));
    assert(calls==0);
    deps[0]=1;
    assert(!mulib_anim_driver_eval(offsets,3,deps,1,2,out,scratch,1,driver,&calls));
    assert(calls==0&&out[0]==42);
    deps[0]=0;
    assert(mulib_anim_driver_eval(offsets,3,deps,1,2,out,scratch,1,driver,&calls));
    assert(calls==2&&out[0]==1&&out[1]==3);
    mulib_mesh_vec3 base[2]={{1,0,0},{2,0,0}},delta[2]={{1,0,0},{NAN,0,0}};
    mulib_mesh_vec3 morph[2]={{42,0,0},{43,0,0}};
    double weight[1]={1};
    assert(!mulib_anim_morph(morph,base,delta,weight,2,1));
    assert(morph[0].x==42&&morph[1].x==43);
    delta[1].x=1;
    assert(mulib_anim_morph(base,base,delta,weight,2,1));
    assert(base[0].x==2&&base[1].x==3);
    mulib_anim_quat q=mulib_anim_quaternion_unit((mulib_anim_quat){DBL_MAX,DBL_MAX,0,0});
    assert(fabs(q.x-sqrt(.5))<1e-15&&q.w==0);
}

static void test_physics(void)
{
    mulib_physics_rigidbody bodies[2]={
        {{0,0,0},{1,0,0},{0,0,0},1,1,1},
        {{1.9,0,0},{-1,0,0},{0,0,0},1,1,1}
    };
    mulib_physics_contact c={0,1,{NAN,0,0},.1,0};
    mulib_physics_rigidbody saved[2];memcpy(saved,bodies,sizeof saved);
    assert(!mulib_physics_solver_contact(bodies,2,&c,.8));
    assert(memcmp(saved,bodies,sizeof saved)==0);
    c.normal=mulib_mesh_v(1,0,0);
    bodies[0].velocity.x=DBL_MAX;bodies[1].velocity.x=-DBL_MAX;
    memcpy(saved,bodies,sizeof saved);
    assert(!mulib_physics_solver_contact(bodies,2,&c,.8));
    assert(memcmp(saved,bodies,sizeof saved)==0&&c.impulse==0);
    bodies[0].velocity.x=1;bodies[1].velocity.x=-1;
    assert(mulib_physics_world_step(bodies,2,mulib_mesh_v(0,0,0),.01,2));
    assert(bodies[0].velocity.x<0&&bodies[1].velocity.x>0);
    bodies[0].inv_mass=DBL_MAX;bodies[1].inv_mass=DBL_MAX;
    memcpy(saved,bodies,sizeof saved);
    assert(!mulib_physics_joint_distance(bodies,bodies+1,1,.5));
    assert(memcmp(saved,bodies,sizeof saved)==0);
    bodies[0].radius=DBL_MAX;bodies[1].radius=DBL_MAX;
    assert(!mulib_physics_narrowphase_sphere(bodies,2,0,1,&c));
}

static void test_bvh(void)
{
    mulib_render_material mat={{1,1,1},{0,0,0},.5,0};
    mulib_render_triangle triangles[9];
    for(size_t i=0;i<9;i++)
        triangles[i]=(mulib_render_triangle){{(double)i,0,0},{i+1.0,0,0},{(double)i,1,0},0};
    mulib_render_scene s={triangles,9,&mat,1,{0,0,0}};
    mulib_render_bvh_node nodes[17];
    size_t ids[9],used=99,stack[17];
    assert(!mulib_render_bvhbuild(s,nodes,16,ids,9,&used)&&used==99);
    assert(mulib_render_bvhbuild(s,nodes,17,ids,9,&used));
    for(unsigned i=0;i<2000;i++){
        double x=(scene_random()%11000)/1000.0-1;
        double y=(scene_random()%2000)/1000.0-.5;
        mulib_render_ray ray={{x,y,1},{0,0,-1}};
        mulib_render_hit brute={0},hit={0};
        bool expected=mulib_render_scene_hit(s,ray,0,10,&brute);
        mulib_render_bvh_result result=mulib_render_bvh_hit_checked(s,nodes,used,ids,9,ray,0,10,stack,17,&hit);
        assert(result==(expected?MULIB_RENDER_BVH_HIT:MULIB_RENDER_BVH_MISS));
        if(expected)assert(fabs(brute.t-hit.t)<1e-12);
    }
    /* Hit first leaf, then fail second leaf: output must remain unchanged. */
    mulib_render_aabb box={mulib_mesh_v(0,0,0),mulib_mesh_v(10,1,0)};
    mulib_render_bvh_node bad[3]={{box,1,2,0,0},{box,0,0,0,1},{box,0,0,1,1}};
    size_t invalid_ids[2]={0,9};
    mulib_render_hit hit={77,0,0,88,{0,0,0}},saved=hit;
    mulib_render_ray ray={{.2,.2,1},{0,0,-1}};
    assert(mulib_render_bvh_hit_checked(s,bad,3,invalid_ids,2,ray,0,10,stack,17,&hit)==MULIB_RENDER_BVH_INVALID);
    assert(memcmp(&hit,&saved,sizeof hit)==0);
    assert(mulib_render_bvh_hit_checked(s,bad,3,invalid_ids,2,ray,0,10,stack,1,&hit)==MULIB_RENDER_BVH_INVALID);
    assert(hit.t==77);
    bad[0].left=0;
    assert(!mulib_render_bvh_hit(s,bad,3,invalid_ids,2,ray,0,10,stack,17,&hit));
    ray.direction.x=NAN;
    assert(!mulib_render_scene_tri_hit(triangles[0],ray,0,10,&hit));
    assert(!mulib_render_scene_tri_hit(triangles[0],(mulib_render_ray){{0,0,1},{0,0,-1}},0,10,NULL));
    mulib_render_film_pixel film={{DBL_MAX,0,0},1};
    assert(!mulib_render_film_add(&film,mulib_mesh_v(DBL_MAX,0,0)));
    assert(film.samples==1&&film.sum.x==DBL_MAX);
}

int main(void)
{
    test_mesh();
    test_triangulate();
    test_geometry();
    test_animation();
    test_physics();
    test_bvh();
    puts("Scene review regressions passed");
    return 0;
}
