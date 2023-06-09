#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <fstream>
#include <vector>
#include <iostream>
#include <cassert>

#if defined __linux__ || defined __APPLE__
  // compiled for linux
#else
// need to define
#define M_PI 3.141592653589793
#define INFINITY 1e8
#endif

template<typename T>
class Vec3 {
public:
  T x, y, z;
  // new zero vector
  Vec3() : x(T(0)), y(T(0)), z(T(0)) {}
  // new unit vector
  Vec3(T xx) : x(xx), y(xx), z(xx) {}
  // new explicit vector
  Vec3(T xx, T yy, T zz) : x(xx), y(yy), z(zz) {}
  // transform to unit vector
  Vec3& normalize() {
    T nor2 = length2();
    if (nor2 > 0) {
      T invNor = 1 / sqrt(nor2);
      x *= invNor, y *= invNor, z *= invNor;
    }
    return *this;
  }
  // scale by constant
  Vec3<T> operator * (const T &f) const { return Vec3<T>(x * f, y * f, z * f); }
  // scale by vector
  Vec3<T> operator * (const Vec3<T> &v) const { return Vec3<T>(x * v.x, y * v.y, z * v.z); }
  // dot product
  T dot(const Vec3<T> &v) const { return x * v.x + y * v.y + z * v.z; }
  // vector subtraction
  Vec3<T> operator - (const Vec3<T> &v) const { return Vec3<T>(x - v.x, y - v.y, z - v.z); }
  // vector addition
  Vec3<T> operator + (const Vec3<T> &v) const { return Vec3<T>(x + v.x, y + v.y, z + v.z); }
  // vector addition assignment
  Vec3<T>& operator += (const Vec3<T> &v) {x += v.x, y += v.y, z += v.z; return *this; }
  // vector multiplication assignment
  Vec3<T>& operator *= (const Vec3<T> &v) {x *= v.x, y *= v.y, z *= v.z; return *this; }
  // vector negation
  Vec3<T> operator - () const { return Vec3<T>(-x, -y, -z); }
  // sum of squares
  T length2() const { return x * x + y * y + z * z; }
  // length between origin and vector
  T length() const { return sqrt(length2()); }
  // pretty print
  friend std::ostream & operator << (std::ostream &os, const Vec3<T> &v) {
    // [x y z]
    os << "[" << v.x << " " << v.y << " " << v.z << "]";
    return os;
  }
};

// v? pixel? point?
typedef Vec3<float> Vec3f;

class Sphere {
public:
  Vec3f center;                         /// position of the sphere
  float radius, radsquared;             /// sphere radius and radius^2
  Vec3f surfaceColor, emissionColor;    /// surface color and emission (light)
  float transparency, reflection;       /// surface transparency and reflectivity
  Sphere(
    const Vec3f &c,
    const float &r,
    const Vec3f &sc,
    const float &refl = 0,
    const float &transp = 0,
    const Vec3f &ec = 0) :
    center(c), radius(r), radsquared(r * r), surfaceColor(sc), emissionColor(ec),
    transparency(transp), reflection(refl) { /* */ }
  // tests whether the ray origin pointing in the ray direction will intersect... what?
  bool intersect(const Vec3f &rayorig, const Vec3f &raydir, float &t0, float &t1) const {
    Vec3f dist = center - rayorig;
    float norm = dist.dot(raydir);
    // if the normal is negative, we're testing from inside the sphere, return false
    if (norm < 0) return false;
    float d2 = dist.dot(dist) - norm * norm;
    // ???
    if (d2 > radsquared) return false;
    float thc = sqrt(radsquared - d2);
    t0 = norm - thc;
    t1 = norm + thc;
    // ???
    return true;
  }
};

#define MAX_RAY_DEPTH 5

// mixing reflection and transparency values?
float mix(const float &a, const float &b, const float &mix) {
  return b * mix + a * (1 - mix);
}

Vec3f trace(
  const Vec3f &rayorig,
  const Vec3f &raydir,
  const std::vector<Sphere> &spheres,
  const int &depth
) {
  // if (raydir.length() != 1) std::cerr << "Error " << raydir << std::endl;
  float tnear = INFINITY;
  const Sphere* sphere = NULL;
  // find intersection of this ray with the sphere in the scene
  for (unsigned i = 0; i < spheres.size(); ++i) {
    float t0 = INFINITY, t1 = INFINITY;
    if (spheres[i].intersect(rayorig, raydir, t0, t1)) {
      if (t0 < 0) t0 = t1;
      if (t0 < tnear) {
        tnear = t0;
        sphere = &spheres[i];
      }
    }
  }
  // if no intersection, return black or background color
  if (!sphere) return Vec3f(2);

  Vec3f surfaceColor = 0;               // color of the ray/surface of object intersected by ray
  Vec3f pointhit = rayorig + raydir * tnear;// point of intersection
  Vec3f normalhit = pointhit - sphere->center;   // normal at the point of intersection

  normalhit.normalize();                     // normalize normal direction


  /*
   if normal and view direction are not opposite to each other, reverse the normal direction
   that also means we are inside the sphere so set the inside bool to true.
   Finally, reverse the sign of IdotN which we want positive.
  */

  float bias = 1e-4;                    // add some bias to the point we're tracing from
  bool inside = false;
  if (raydir.dot(normalhit) > 0) normalhit = -normalhit, inside = true;
  if ((sphere->transparency > 0 || sphere->reflection > 0) && depth < MAX_RAY_DEPTH) {
    float facingratio = -raydir.dot(normalhit);
    // change the mix value to tweak the effect
    float fresneleffect = mix(pow(1 - facingratio, 3), 1, .01);
    // compute reflection direction (no need to normalize b/c all vectors are already normalized)
    Vec3f refldir = raydir - normalhit * 2 * raydir.dot(normalhit);
    refldir.normalize();
    Vec3f reflection = trace(pointhit + normalhit * bias, refldir, spheres, depth + 1);
    Vec3f refraction = 0;
    // if the sphere is also transparent compute refraction ray (transmission)
    if (sphere->transparency) {
      float ior = 1.1, eta = (inside) ? ior : 1 / ior; // inside or outside?
      float cosi = -normalhit.dot(raydir);
      float k = 1 - eta * eta * (1 - cosi * cosi);
      Vec3f refrdir = raydir * eta + normalhit * (eta * cosi - sqrt(k));
      refrdir.normalize();
      refraction = trace(pointhit - normalhit * bias, refrdir, spheres, depth + 1);
    }
    // the result is a mix of reflection and refraction (if transparent)
    surfaceColor = (
      reflection * fresneleffect +
      refraction * (1 - fresneleffect) * sphere->transparency) * sphere->surfaceColor;
  }
  else {
    // it's a diffuse object, no need to raytrace any further
  for (unsigned i = 0; i < spheres.size(); ++i) {
    if (spheres[i].emissionColor.x > 0) {
      // this is a light
      Vec3f transmission = 1;
      Vec3f lightDirection = spheres[i].center - pointhit;
      lightDirection.normalize();
      for (unsigned j = 0; j < spheres.size(); ++j) {
        if (i != j) {
          float t0, t1;
          if (spheres[j].intersect(pointhit + normalhit * bias, lightDirection, t0, t1)) {
            transmission = 0;
            break;
          }
        }
      }
      surfaceColor += sphere->surfaceColor * transmission * std::max(float(0), normalhit.dot(lightDirection)) * spheres[i].emissionColor;
      }
    }
  }
  return surfaceColor + sphere->emissionColor;
}

/*
  main rendering function
  compute camera ray for each pixel of the image
  trace it and return a color
  if the ray hits a sphere, we return the color of the sphere at the intersection point
  otherwise, we return the background color
*/

void render(const std::vector<Sphere> &spheres) {
  unsigned width = 640, height = 480;
  Vec3f *image = new Vec3f[width * height], *pixel = image;
  float invWidth = 1 / float(width), invHeight = 1 / float(height);
  float fov = 30, aspectratio = width / float(height);
  float angle = tan(M_PI * 0.5 * fov / 180.);
  // trace rays
  for (unsigned y = 0; y < height; ++y) {
    for (unsigned x = 0; x < width; ++x, ++pixel) {
      float xx = (2 * ((x + 0.5) * invWidth) - 1) * angle * aspectratio;
      float yy = (1 - 2 * ((y + 0.5) * invHeight)) * angle;
      Vec3f raydir(xx, yy, -1);
      raydir.normalize();
      *pixel = trace(Vec3f(0), raydir, spheres, 0);
    }
  }
  // save result to a PPM image (might need to lose this if not compiled on Windows)
  std::ofstream ofs("./untitled.ppm", std::ios::out | std::ios::binary);
  ofs << "P6\n" << width << " " << height << "\n255\n";
  for (unsigned i = 0; i < width * height; ++i) {
    ofs << (unsigned char)(std::min(float(1), image[i].x) * 255) <<
           (unsigned char)(std::min(float(1), image[i].y) * 255) <<
           (unsigned char)(std::min(float(1), image[i].z) * 255);
  }
  ofs.close();
  delete [] image;
}

int main(int argc, char **argv) {
  srand48(13);
    std::vector<Sphere> spheres;
    // position, radius, surface color, reflectivity, transparency, emission color
    spheres.push_back(Sphere(Vec3f( 0.0, -10004, -20), 10000, Vec3f(0.20, 0.20, 0.20), 0, 0.0));
    spheres.push_back(Sphere(Vec3f( 0.0,      0, -20),     4, Vec3f(1.00, 0.32, 0.36), 1, 0.5));
    spheres.push_back(Sphere(Vec3f( 5.0,     -1, -15),     2, Vec3f(0.90, 0.76, 0.46), 1, 0.0));
    spheres.push_back(Sphere(Vec3f( 5.0,      0, -25),     3, Vec3f(0.65, 0.77, 0.97), 1, 0.0));
    spheres.push_back(Sphere(Vec3f(-5.5,      0, -15),     3, Vec3f(0.90, 0.90, 0.90), 1, 0.0));
    // light
    spheres.push_back(Sphere(Vec3f( 0.0,     20, -30),     3, Vec3f(0.00, 0.00, 0.00), 0, 0.0, Vec3f(3)));
    render(spheres);

    return 0;
}


