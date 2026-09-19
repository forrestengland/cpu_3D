#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#define WIDTH 800
#define HEIGHT 600
#define PI 3.1415926535

#define CAMERA_DISTANCE 7.0;

typedef struct {
  float x, y, z;
} Vec3;

typedef struct {
  int v[3];
  //  SDL_Color color;
} Face;

typedef struct {
  int face_index;
  float avg_z;
} SortedFace;

Vec3* vertices = NULL;
int vertex_count = 0;
int vertex_capacity = 0;

Face* faces = NULL;
int face_count = 0;
int face_capacity = 0;

Vec3 normalize_vector(Vec3 v) {
  float length = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
  if (length == 0.0) {
    return (Vec3){0.0,0.0,0.0};
  }
  return (Vec3){v.x / length, v.y / length, v.z / length};
}

void add_vertex(Vec3 v) {

  if (vertex_count >= vertex_capacity) {
    vertex_capacity = vertex_capacity == 0 ? 64 : vertex_capacity * 2;
    vertices = realloc(vertices, vertex_capacity * sizeof(Vec3));
  }

  int vi = vertex_count;
  vertex_count++;
  
  //  vertices[vertex_count++] = v;
  vertices[vi].x = v.x;
  vertices[vi].y = v.y;
  vertices[vi].z = v.z;
}

void add_face(Face f) {

  if (face_count >= face_capacity) {
    if (face_capacity == 0) {
      face_capacity = 64;
    } else {
      face_capacity = face_capacity * 2;
    }
    faces = realloc(faces, face_capacity * sizeof(Face));
  }

  // assign random color
  /*  f.color.r = rand() % 200 + 55;
  f.color.g = rand() % 200 + 55;
  f.color.b = rand() % 200 + 55;
  f.color.a = 255; */

  int fi = face_count;
  face_count++;

  faces[fi].v[0] = f.v[0];
  faces[fi].v[1] = f.v[1];
  faces[fi].v[2] = f.v[2];
  /*  faces[fi].color.r = f.color.r;
  faces[fi].color.g = f.color.g;
  faces[fi].color.b = f.color.b;
  faces[fi].color.a = f.color.a; */
}

int load_obj(const char* filename) {

  FILE* file = fopen(filename, "r");
  if (!file) {
    printf("failed to open file %s\n", filename);
    return 0;
  }

  char line[256];
  while (fgets(line, sizeof(line), file)) {
    if (line[0] == 'v' && line[1] == ' ') {
      Vec3 v;
      sscanf(line, "v %f %f %f", &v.x, &v.y, & v.z);
      add_vertex(v);
      
      /*    } else if (line[0] == 'f' && line[1] == ' ') {
      Face f;
      sscanf(line, "f %d %d %d", &f.v[0], &f.v[1], &f.v[2]);
      f.v[0]--;
      f.v[1]--;
      f.v[2]--;
      add_face(f);*/
    } else if (line[0] == 'f' && line[1] == ' ') {

      int v[4];
      int n[4];

      int count = sscanf(
			 line,
			 "f %d//%d %d//%d %d//%d %d//%d",
			 &v[0], &n[0],
			 &v[1], &n[1],
			 &v[2], &n[2],
			 &v[3], &n[3]
			 );

      if (count == 6) {
        /*
         * Triangle
         */
        Face f;

        f.v[0] = v[0] - 1;
        f.v[1] = v[1] - 1;
        f.v[2] = v[2] - 1;

        add_face(f);
      } else if (count == 8) {
        /*
         * Quad:
         *
         * 0 ----- 1
         * |       |
         * |       |
         * 3 ----- 2
         *
         * split into:
         *
         * 0, 1, 2
         * 0, 2, 3
         */

        Face f1;
        f1.v[0] = v[0] - 1;
        f1.v[1] = v[1] - 1;
        f1.v[2] = v[2] - 1;

        add_face(f1);

        Face f2;
        f2.v[0] = v[0] - 1;
        f2.v[1] = v[2] - 1;
        f2.v[2] = v[3] - 1;

        add_face(f2);
      }
    }
  }
  
  fclose(file);
  printf("loaded obj: %d vertices, %d faces\n", vertex_count, face_count);
  return 1;
}

int compare_faces(const void* a, const void* b) {
  SortedFace* face_a = (SortedFace*)a;
  SortedFace* face_b = (SortedFace*)b;

  if (face_a->avg_z < face_b->avg_z) return 1;
  if (face_a->avg_z > face_b->avg_z) return -1;
  return 0;
}

void process_render(SDL_Renderer* renderer, float angle) {

  float rad = angle * (PI / 180.0);
  float cos_a = cosf(rad);
  float sin_a = sinf(rad);

  Vec3 light_direction = {1.0,1.0,-1.0};
  light_direction = normalize_vector(light_direction);

  float base_r = 0.0;
  float base_g = 180.0;
  float base_b = 200.0;

  float ambient_light = 0.15;

  Vec3* transformed = malloc(vertex_count * sizeof(Vec3));

  for (int i=0; i<vertex_count; i++) {

    Vec3 v = vertices[i];

    // y rotation
    float x1 = v.x * cos_a - v.z * sin_a;
    float z1 = v.x * sin_a + v.z * cos_a;

    // x rotation
    float y2 = v.y * cos_a - z1 * sin_a;
    float z2 = v.y * sin_a + z1 * cos_a;

    // shift z location
    transformed[i].x = x1;
    transformed[i].y = y2;
    transformed[i].z = z2 + CAMERA_DISTANCE; // camera distance
  }

  SortedFace* sorted_list = malloc(face_count * sizeof(SortedFace));
  int visible_face_count = 0;

  // index face
  for (int i=0; i<face_count; i++) {
    Vec3 p0 = transformed[faces[i].v[0]];
    Vec3 p1 = transformed[faces[i].v[1]];
    Vec3 p2 = transformed[faces[i].v[2]];

    // calculate surface vector normal
    Vec3 edge1 = {p1.x - p0.x, p1.y - p0.y, p1.z - p0.z};
    Vec3 edge2 = {p2.x - p0.x, p2.y - p0.y, p2.z - p0.z};

    // cross product normal calculation
    Vec3 normal;
    normal.x = edge1.y * edge2.z - edge1.z * edge2.y;
    normal.y = edge1.z * edge2.x - edge1.x * edge2.z;
    normal.z = edge1.x * edge2.y - edge1.y * edge2.x;

    Vec3 cameraRay = {p0.x, p0.y, p0.z};

    float dot = normal.x * cameraRay.x +
      normal.y * cameraRay.y +
      normal.z * cameraRay.z;

    // backface culling
    if (dot < 0.0f) {

      sorted_list[visible_face_count].face_index = i;
      sorted_list[visible_face_count].avg_z = (p0.z + p1.z + p2.z) / 3.0;
      visible_face_count++;

    }
  }

  qsort(sorted_list, visible_face_count, sizeof(SortedFace), compare_faces);

  //    SDL_FPoint screen[3];
  float fov = 600.0;

  for (int i=0; i<visible_face_count; i++) {

    Face f = faces[sorted_list[i].face_index];

    Vec3 p0 = transformed[f.v[0]];
    Vec3 p1 = transformed[f.v[1]];
    Vec3 p2 = transformed[f.v[2]];

    // recalculate normal for lighting
    Vec3 edge1 = {p1.x - p0.x, p1.y - p0.y, p1.z - p0.z};
    Vec3 edge2 = {p2.x - p0.x, p2.y - p0.y, p2.z - p0.z};
    Vec3 normal = {
      edge1.y * edge2.z - edge1.z * edge2.y,
      edge1.z * edge2.x - edge1.x * edge2.z,
      edge1.x * edge2.y - edge1.y * edge2.x
    };
    normal = normalize_vector(normal);
    float light_intensity = normal.x * light_direction.x + normal.y * light_direction.y + normal.z * light_direction.z;
    if (light_intensity < 0.0f) light_intensity = 0.0;

    float total_shade = ambient_light + light_intensity;
    if (total_shade > 1.0) total_shade = 1.0;
    
    SDL_Vertex sdl_vertices[3];
    
    for (int j=0; j<3; j++) {
      Vec3 p = transformed[f.v[j]];
      
      //screen[j].x = (p.x * fov) / p.z + (WIDTH / 2.0);
      //      screen[j].y = (p.y * fov) / p.z + (HEIGHT / 2.0);

      sdl_vertices[j].position.x = (p.x * fov) / p.z + (WIDTH / 2.0);
      sdl_vertices[j].position.y = (p.y * fov) / p.z + (HEIGHT / 2.0);

      sdl_vertices[j].color.r = (base_r * total_shade) / 255.0;
      sdl_vertices[j].color.g = (base_g * total_shade) / 255.0;
      sdl_vertices[j].color.b = (base_b * total_shade) / 255.0;
      sdl_vertices[j].color.a = 1.0;

      // disregard uv map parameters for flat fill
      sdl_vertices[j].tex_coord.x = 0.0;
      sdl_vertices[j].tex_coord.y = 0.0;
    }

    SDL_RenderGeometry(renderer, NULL, sdl_vertices, 3, NULL, 0);

    // draw wireframe
    /*    SDL_RenderLine(renderer, screen[0].x, screen[0].y,
		   screen[1].x, screen[1].y);
    SDL_RenderLine(renderer, screen[1].x, screen[1].y,
		   screen[2].x, screen[2].y);
    SDL_RenderLine(renderer, screen[2].x, screen[2].y,
    screen[0].x, screen[0].y); */
      //    }
  }

  free(sorted_list);
  free(transformed);
}

int main(int argc, char* argv[]) {

  srand((unsigned int)time(NULL));

  SDL_Init(SDL_INIT_VIDEO);

  
  //  load_obj("cube.obj");
  //  load_obj("teapot.obj");
  //    load_obj("mactri.obj");
  load_obj("shape.obj");

  SDL_Window* window = SDL_CreateWindow("3d", WIDTH, HEIGHT, 0);
  SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);

  int running = 1;
  float angle = 0.0;

  while (running) {

    SDL_Event event;

    while (SDL_PollEvent(&event)) {

      if (event.type == SDL_EVENT_QUIT) running = 0;
    }

    SDL_SetRenderDrawColor(renderer, 10, 14, 22, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 0, 255, 180, 255);
    process_render(renderer, angle);

    SDL_RenderPresent(renderer);
    angle += 0.8;
    SDL_Delay(16);
  }

  free(vertices);
  free(faces);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  
  return 0;
}
