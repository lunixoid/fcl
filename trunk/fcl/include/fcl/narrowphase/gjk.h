/*
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2011, Willow Garage, Inc.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of Willow Garage, Inc. nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/** \author Jia Pan */

#ifndef FCL_GJK_H
#define FCL_GJK_H

#include "fcl/geometric_shapes.h"
#include "fcl/matrix_3f.h"
#include "fcl/transform.h"

namespace fcl
{

namespace details
{

Vec3f getSupport(const ShapeBase* shape, const Vec3f& dir); 

struct MinkowskiDiff
{
  const ShapeBase* shapes[2];
  Matrix3f toshape1;
  Transform3f toshape0;

  MinkowskiDiff() { }

  inline Vec3f support0(const Vec3f& d) const
  {
    return getSupport(shapes[0], d);
  }

  inline Vec3f support1(const Vec3f& d) const
  {
    return toshape0.transform(getSupport(shapes[1], toshape1 * d));
  }

  inline Vec3f support(const Vec3f& d) const
  {
    return support0(d) - support1(-d);
  }

  inline Vec3f support(const Vec3f& d, size_t index) const
  {
    if(index)
      return support1(d);
    else
      return support0(d);
  }

};


FCL_REAL projectOrigin(const Vec3f& a, const Vec3f& b, FCL_REAL* w, size_t& m);

FCL_REAL projectOrigin(const Vec3f& a, const Vec3f& b, const Vec3f& c, FCL_REAL* w, size_t& m);

FCL_REAL projectOrigin(const Vec3f& a, const Vec3f& b, const Vec3f& c, const Vec3f& d, FCL_REAL* w, size_t& m);

struct GJK
{
  struct SimplexV
  {
    Vec3f d; // support direction
    Vec3f w; // support vector
  };

  struct Simplex
  {
    SimplexV* c[4]; // simplex vertex
    FCL_REAL p[4]; // weight
    size_t rank; // size of simplex (number of vertices)
  };

  enum Status {Valid, Inside, Failed};

  MinkowskiDiff shape;
  Vec3f ray;
  FCL_REAL distance;
  Simplex simplices[2];


  GJK(unsigned int max_iterations_, FCL_REAL tolerance_) 
  {
    max_iterations = max_iterations_;
    tolerance = tolerance_;

    initialize(); 
  }
  
  void initialize();

  Status evaluate(const MinkowskiDiff& shape_, const Vec3f& guess);

  void getSupport(const Vec3f& d, SimplexV& sv) const;

  void removeVertex(Simplex& simplex);

  void appendVertex(Simplex& simplex, const Vec3f& v);

  bool encloseOrigin();

  inline Simplex* getSimplex() const
  {
    return simplex;
  }
  
private:
  SimplexV store_v[4];
  SimplexV* free_v[4];
  size_t nfree;
  size_t current;
  Simplex* simplex;
  Status status;

  FCL_REAL tolerance;
  unsigned int max_iterations;
};


static const size_t EPA_MAX_FACES = 128;
static const size_t EPA_MAX_VERTICES = 64;
static const FCL_REAL EPA_EPS = 0.000001;
static const size_t EPA_MAX_ITERATIONS = 255;

struct EPA
{
private:
  typedef GJK::SimplexV SimplexV;
  struct SimplexF
  {
    Vec3f n;
    FCL_REAL d;
    SimplexV* c[3]; // a face has three vertices
    SimplexF* f[3]; // a face has three adjacent faces
    SimplexF* l[2]; // the pre and post faces in the list
    size_t e[3];
    size_t pass;
  };

  struct SimplexList
  {
    SimplexF* root;
    size_t count;
    SimplexList() : root(NULL), count(0) {}
    void append(SimplexF* face)
    {
      face->l[0] = NULL;
      face->l[1] = root;
      if(root) root->l[0] = face;
      root = face;
      ++count;
    }

    void remove(SimplexF* face)
    {
      if(face->l[1]) face->l[1]->l[0] = face->l[0];
      if(face->l[0]) face->l[0]->l[1] = face->l[1];
      if(face == root) root = face->l[1];
      --count;
    }
  };

  static inline void bind(SimplexF* fa, size_t ea, SimplexF* fb, size_t eb)
  {
    fa->e[ea] = eb; fa->f[ea] = fb;
    fb->e[eb] = ea; fb->f[eb] = fa;
  }

  struct SimplexHorizon
  {
    SimplexF* cf; // current face in the horizon
    SimplexF* ff; // first face in the horizon
    size_t nf; // number of faces in the horizon
    SimplexHorizon() : cf(NULL), ff(NULL), nf(0) {}
  };

private:
  unsigned int max_face_num;
  unsigned int max_vertex_num;
  unsigned int max_iterations;
  FCL_REAL tolerance;

public:

  enum Status {Valid, Touching, Degenerated, NonConvex, InvalidHull, OutOfFaces, OutOfVertices, AccuracyReached, FallBack, Failed};
  
  Status status;
  GJK::Simplex result;
  Vec3f normal;
  FCL_REAL depth;
  SimplexV* sv_store;
  SimplexF* fc_store;
  size_t nextsv;
  SimplexList hull, stock;

  EPA(unsigned int max_face_num_, unsigned int max_vertex_num_, unsigned int max_iterations_, FCL_REAL tolerance_)
  {
    max_face_num = max_face_num_;
    max_vertex_num = max_vertex_num_;
    max_iterations = max_iterations_;
    tolerance = tolerance_;

    initialize();
  }

  ~EPA()
  {
    delete [] sv_store;
    delete [] fc_store;
  }

  void initialize();

  bool getEdgeDist(SimplexF* face, SimplexV* a, SimplexV* b, FCL_REAL& dist);

  SimplexF* newFace(SimplexV* a, SimplexV* b, SimplexV* c, bool forced);

  /** \brief Find the best polytope face to split */
  SimplexF* findBest();

  Status evaluate(GJK& gjk, const Vec3f& guess);

  /** \brief the goal is to add a face connecting vertex w and face edge f[e] */
  bool expand(size_t pass, SimplexV* w, SimplexF* f, size_t e, SimplexHorizon& horizon);  
};


} // details




}


#endif