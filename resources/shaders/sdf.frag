#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UniformBufferObject {
	vec3 res;
	float time;
  mat4 projection;
  mat4 view;
} ubo;

struct Ray
{
  vec4 pos;
  vec4 dir;
};

struct Hit{
  float dist; //distance
  float mtl;  // material id
  vec3 hit;   // hit point;
};
layout(location = 0) in vec3 iPosition;
layout(location = 1) in vec2 fragCoord;
layout(location = 2) in vec3 iResolution;
layout(location = 3) in float iGlobalTime;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outPosition;
layout(location = 2) out vec4 outNormals;

#define PI 3.14159265359


// PRIMITIVES
/*
* r - radius of sphere
*/
float sdfSphere(vec3 p, float r) 
{
  return length(p) - r;
}

/*
* b - vector with x:width, y:height and z:depth of box
*/
float sdfBox( vec3 p, vec3 b )
{
  vec3 d = abs(p) - b;
  return min(max(d.x,max(d.y,d.z)),0.0) + length(max(d,0.0));
}

/*
* b - vector with x:width, y:height and z:depth of box
* r - smoothing factor
*/
float sdfRoundBox( vec3 p, vec3 b, float r ) //eigentlich unsigned
{
  return length(max(abs(p)-b,0.0))-r;
}

/*
* t - vector with x:outer radius and y:inner radius
*/
float sdfTorus( vec3 p, vec2 t )
{
  vec2 q = vec2(length(p.xy)-t.x,p.z); // opening facing along z axis
  // vec2(length(p.xz)-t.x,p.y) --> opening facing along y axis
  // vec2(length(p.yz)-t.x,p.x) --> opening facing along x axis
  return length(q)-t.y;
}

/*
* c - vector with x:radius and y:lenght
*/
float sdfCylinder(vec3 p, vec2 c) {
	float d = length(p.yz) - c.x;
	d = max(d, abs(p.x) - c.y);
	return d;
}

/*
* c - vector with x:height and y:base radius
*/
float sdfCone( vec3 p, vec2 c )
{
  vec2 q = vec2(length(p.xz), -p.y);
	vec2 tip = q - vec2(0, c.x);
	vec2 mantleDir = normalize(vec2(c.x, c.y));
	float mantle = dot(tip, mantleDir);
	float d = max(mantle, -q.y);
	float projected = dot(tip, vec2(mantleDir.y, -mantleDir.x));
	
	// distance to tip
	if ((q.y > c.x) && (projected < 0)) {
		d = max(d, length(tip));
	}
	
	// distance to base ring
	if ((q.x > c.y) && (projected > length(vec2(c.x, c.y)))) {
		d = max(d, length(q - vec2(c.y, 0)));
	}
	return d;
}

/*
* n - vector with xyz:plane normal and w:distance from origin
*/
float sdfPlane(vec3 p, vec4 n) {
	return dot(p, n.xyz) + n.w;
}

/*
* h - vector with x:radius and y: depth
*/
float sdfHexPrism( vec3 p, vec2 h )
{
    vec3 q = abs(p);
    return max(q.z-h.y,max((q.x*0.866025+q.y*0.5),q.y)-h.x);
}

/*
* h - vector with x:radius and y:length
*/
float sdfTriPrism( vec3 p, vec2 h )
{
    vec3 q = abs(p);
    return max(q.z-h.y,max(q.x*0.866025+p.y*0.5,-p.y)-h.x*0.5);
}

/*
* r - radius
* c - ??
*/
float sdfCapsule(vec3 p, float r, float c)
{
	return mix(length(p.xz) - r, length(vec3(p.x, abs(p.y) - c, p.z)) - r, step(c, abs(p.y)));
}

/*
* h - vector with x: x of base, y: height and z: z of base (square base --> base x = base z)
*/
float sdfPyramid4( vec3 p, vec3 h ) {
        p.xz = abs(p.xz);                   // Symmetrical about XY and ZY
        vec3 n = normalize(h);
        return sdfPlane(p, vec4( n, 0.0 ) ); // cut off bottom
}


//OBJECT COMBINATION
/*
* dist1 - distance to first object
* dist2 - distance to second object
*/
float opUnion(float dist1, float dist2) 
{
  return min(dist1, dist2);
}

float opIntersection(float dist1, float dist2) 
{
  return max(dist1, dist2);
}

/*
* Note: the second object is subtracted from the first --> first object will be manipulated
*       same for opDifferenceRound
*/
float opDifference(float dist1, float dist2) 
{
  return max(-dist1, dist2);
}

/*
* r - smoothing factor
*/
float opUnionRound(float dist1, float dist2, float r) {
	vec2 u = max(vec2(r - dist1,r - dist2), vec2(0));
	return max(r, min (dist1, dist2)) - length(u);
}

float opIntersectionRound(float dist1, float dist2, float r) {
	vec2 u = max(vec2(r + dist1,r + dist2), vec2(0));
	return min(-r, max (dist1, dist2)) + length(u);
}

float opDifferenceRound (float dist1, float dist2, float r) {
	return opIntersectionRound(-dist1, dist2, r);
}



// DOMAIN MANIPULATION
/*
* Well, it does not do, what one would think it shoul do. Still here because of amusement factor
* p - vector with plane you want to rotate (eg. p = vec2(point.x, poit.z) -- > rotate xz-plane around y-axis)
* a - rotation angle;
*/
void funkyrotate(inout vec2 p, float a) {
	p = cos(a)*p + sin(a)*vec2(p.y, -p.x);
}

vec3 opRot( vec3 p, mat3 m )
{
    vec3 q = inverse(m)*p;
    return q;
}

vec3 opRep( vec3 p, vec3 c )
{
    vec3 q = mod(p,c)-0.5*c;
    return q;
}

mat3 createRotMat(float x, float y, float z) {
  return mat3(cos(y)*cos(z), cos(z)*sin(x)*sin(y)-cos(x)*sin(z), cos(x)*cos(z)*sin(y)+sin(x)*sin(z),
              cos(y)*sin(z), cos(x)*cos(z)+sin(x)*sin(y)*sin(z), cos(x)*sin(y)*sin(z)-cos(z)*sin(x),
              -sin(y),       cos(y)*sin(x),                      cos(x)*cos(y));
}


// (OB)SCENE
float scene(vec3 p) {
  float hit8 = sdfSphere(p - vec3(0.0,0.0 + 0.5 * sin(ubo.time),-5.0 + 1.0 * sin(ubo.time * 0.5)), 0.7 );
  vec2 torus = vec2(2.0, 0.5);
  float hit4 = sdfTorus(p - vec3(0.0,0.0,-5.0 + 1.0 * sin(ubo.time * 0.5)), torus);
  float hit5 = sdfTorus(p - vec3(0.0,0.0,-4.0 + 1.0 * sin(ubo.time * 0.5)), torus);
  float hit6 = sdfTorus(p - vec3(0.0,0.0,-6.0 + 1.0 * sin(ubo.time * 0.5)), torus);
  mat3 rot = createRotMat(PI * 0.5, 0.0, 0.0);
  vec3 r = opRot(p, rot);
  float hit3 = sdfTorus(r - vec3(0.0,5.0 - 1.0 * sin(ubo.time * 0.5),0.0 + 1.0 * sin(ubo.time * 0.5)), torus);
  mat3 rot2 = createRotMat(0.0, 0.0, PI * ubo.time);
  vec3 r2 = opRot(p, rot2);
  float hit9 = sdfSphere(r2 - vec3(0.0,-4.0,-5.0), 0.5 );
  mat3 rot3 = createRotMat(PI * ubo.time, 0.0, 0.0);
  vec3 p2 = p - vec3(0.0, -4.0, -5.0);
  vec3 r4 = opRot(p2, rot3);
  vec3 p3 = r4 - vec3(0.0, -4.0, 5.0); 
  vec3 r3 = opRot(p3, rot2);
  vec2 torus2 = vec2(0.7, 0.1);
  float hit10 = sdfTorus(r3 - vec3(0.0,4.0,-5.0), torus2);
  // vec3 box = vec3(3.0, 1.0, 1.0);
  // float hit2 = sdfBox(p - vec3(0.0,-0.4,-5.0), box);
  vec2 cone = vec2(30.0, 10.0);
  float hit14 = sdfCone(p - vec3(-30.0,10.0,-100.0), cone);
  vec2 hp = vec2(10.0, 10.0);
  float hit12 = sdfTriPrism(p - vec3(-20.0,40.0,-50.0), hp);
  vec3 p6 = p - vec3(-20.0,5.0,-80.0);
  p6 = opRot(p6, rot);
  hit12 = opUnion(hit12, sdfTriPrism(p6, hp));
  mat3 rot4 = createRotMat(0.0, 0.0, -PI * ubo.time * .5);
  mat3 rot5 = createRotMat(0.0, 0.0, PI * ubo.time * .5); 
  vec3 p4 = opRot(p, rot5);
  vec3 p5 = opRot(p, rot4);
  float hit11 = sdfCapsule(p4 - vec3(0.0,4.0,-6.0), 0.2, 0.4);
  hit11 = opUnion(hit11, sdfCapsule(p4 - vec3(0.0,-4.0,-6.0), 0.2, 0.4));
  hit11 = opUnion(hit11, sdfCapsule(p5 - vec3(0.0,-4.0,-4.0), 0.2, 0.4));
  hit11 = opUnion(hit11, sdfCapsule(p5 - vec3(0.0,4.0,-4.0), 0.2, 0.4));
  hit12 = opDifferenceRound(hit12, sdfSphere(p - vec3(-30.0, 50.0, -100.0) ,50.0), 10.0);
  hit12 = opUnionRound(hit12, sdfSphere(p - vec3(-50.0, 40.0, -60.0), 10.0), 5.0);
  hit12 = opUnionRound(hit14,opUnionRound(hit12, sdfSphere(p - vec3(-60.0, 20.0, -65.0), 10.0), 5.0), 2.5);
  vec2 cyl = vec2(1.0, 2.0);
  float hit13 = sdfCylinder(opRot(p, rot5) - vec3(0.0,0.0,-10.0), cyl);
  hit13 = opUnion(hit13,sdfCylinder(opRot(p, rot4) - vec3(0.0,0.0,-12.0), cyl));
  hit13 = opUnion(hit13,sdfCylinder(opRot(p, rot5) - vec3(0.0,0.0,-14.0), cyl));  
  float hit7 = opUnion(opUnion(hit4, hit5), hit6);
  float hit = opUnion(hit14,opUnion(hit13,opUnion(hit12,opUnion(hit11,opUnion(hit10,opUnion(hit9,opUnion(hit8,opUnion(hit3, hit7))))))));
  // float hit = opDifference(hit1, hit2);
  return hit;
}


// THE REAL SLIM SHADING
vec3 calcNorm(vec3 p) {
  vec2 e = vec2(0.005, 0.0);
  return normalize(vec3(scene(p + e.xyy) - scene(p - e.xyy),
                        scene(p + e.yxy) - scene(p - e.yxy),
                        scene(p + e.yyx) - scene(p - e.yyx)));
}

vec4 slimshady(vec3 eye, vec3 rdir, vec3 norm, vec3 light, Hit hit) {
  vec4 color = vec4(0.0,0.0,0.0,1.0);
  if(-1.0 == hit.dist) {
    return color;
  }
  vec3 ldir = normalize(light - hit.hit);
  // float diffuse = max(0.0, dot(norm, ldir));
  // float spec = max(0.0,dot(reflect(ldir,norm),normalize(dir)));
  // spec = pow(spec, 16.0)*.5;
  vec4 ambientC = vec4((norm + vec3(1.0)) * 0.5, 1.0);
  vec4 diffuseC = vec4(0.7,0.7,0.65, 1.0);
  vec4 specularC = vec4(1.0, 1.0, 1.0, 1.0);

  vec4 ambientL = vec4(0.3, 0.3, 0.3, 1.0);
  vec4 diffuseL = vec4(0.8, 0.8, 0.9, 1.0);
  vec4 specularL = vec4(0.5, 0.5, 0.5, 1.0);
  float shininess = 124;

  vec4 ambient = ambientC * ambientL;

  float lambert = max(dot(norm, ldir), 0);
  vec4 diffuse = diffuseC * diffuseL * lambert;

  float spec = 0.0;

  if (lambert > 0) {
      vec3 halfDir = normalize(rdir + ldir);
      float angle = max(dot(norm, halfDir), 0.0);
      spec = pow(angle, shininess);
  }

  vec4 specular = specularC * spec;

  color = ambient + diffuse;
  return ambient;
}


// ACTUAL TRACING
Hit raymarch(inout Ray ray)
{
  vec3 hit;
  for(int i = 0; i < 100; i++) {
    float dist = scene(ray.pos.xyz);
    ray.pos += dist * ray.dir;
    hit = ray.pos.xyz;
    if (dist < 0.0001) {
      return Hit(dist, 1.0, hit);
    }
  }
  discard;
  return Hit(-1.0, 0.0, hit);
}

Ray getRay(vec3 position)
{
  vec4 cam_proj = ubo.projection * vec4(0.0, 0.0, -0.1, 1.0);
  cam_proj /= cam_proj.w;
  vec4 cam_world = inverse(ubo.view) * vec4(0.0, 0.0, 0.0, 1.0);
  
  // float aspect = ubo.res.x/ubo.res.y;
  // vec4 dir = vec4(position.xy, -2.41421, 0.0);
  // if(aspect>1) {
  //   dir.x *= aspect;
  // } else {
  //   dir.y /= aspect;
  // }
  vec4 pos_proj = vec4(position.xy, 0.0, 1.0);
  pos_proj = inverse(ubo.view) * inverse(ubo.projection) * pos_proj;
  vec4 pos_world = pos_proj / pos_proj.w;
  return Ray(cam_world, normalize(pos_world - cam_world));
}


// INSANE IN THE MAIN
void main()
{
	outColor = vec4(1.0, 0.0, 0.0, 1.0);
	return;
  Ray ray = getRay(iPosition);
  vec3 eye = ray.pos.xyz;
  Hit hit = raymarch(ray);
  vec3 light = vec3(10.5, 20.5, -50.0);
  // if(-1.0 == hit.dist) {
  //   outColor = vec4(0.0, 0.0, 0.0, 1.0);
  // } else {
  vec3 n = calcNorm(hit.hit);
  vec4 color = slimshady(eye, ray.dir.xyz, n, light, hit);
  // float color = abs(ray.pos.z);
  // outColor = vec4(vec3(1.0 - color), 1.0);
  // if(0.0 > color) {
  //   outColor = vec4(1.0, 0.5, 0.0, 1.0);
  // }
  // if(0.0 == color) {
  //   outColor = vec4(1.0, 0.1, 0.5, 1.0);
  // }
  float depth = 1.0 / abs(ray.pos.z);
  depth = (-vec4(ubo.view * ray.pos).z - 0.1) / 99.9;
  gl_FragDepth = depth;
  outColor = color; //color;
  outPosition = vec4((ubo.view * ray.pos).xyz, 1.0);
  outNormals = vec4((ubo.view * vec4(n, 0.0)).xyz, 0.0);
}
