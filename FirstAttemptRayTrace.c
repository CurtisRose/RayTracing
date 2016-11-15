#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>


// Structs
typedef struct { // Object
  char *type;
  double *diffuseColor;
  double *specularColor;
  double reflectivity;
  double refractivity;
  double ior;
  double *position;
  double *normal;
  double radius;
} Object;

typedef struct { // Light
  char *type;
  double *color;
  double *position;
  double *direction;
  double radialA2;
  double radialA1;
  double radialA0;
  double angularA0;
  double theta;
} Light;

typedef struct { // Scene
  // List of Objects
  Object object[128];
  // List of lights
  Light light[128];
  // View screen width and height (in pixels and coordinates)
  double width;
  double height;
  int pixelWidth;
  int pixelHeight;
} Scene;

typedef struct Pixel{ // Pixel (color)
  double red;
  double green;
  double blue;
} Pixel;

typedef struct Position{ // Position
  double x;
  double y;
  double z;
} Position;


// Functions
int next_c(FILE* json);
void expect_c(FILE* json, int d);
void skip_ws(FILE* json);
char* next_string(FILE* json);
double next_number(FILE* json);
double* next_vector(FILE* json);
void read_scene(char* filename);
void printScene();
void raycast();
struct Pixel shade(double *startPosition, double *lookUVector, int recursionLevel);
void displayViewPlane();
void writePpmImage(char *outFilename, int format);
void unitVector(double *vector, double *unitVector);
double tClosestApproachSphere(double *vector, double *position);
double tClosestApproachPlane(double *normal, double *origin, double *position, double *lookUVector);
void vectorMultiply(double *inVector, double *outVector, double mult);
void vectorDivide(double *inVector, double *outVector, double divide);
void vectorSubtract(double *inVector1, double *inVector2, double *outVector);
void vectorAddition(double *inVector1, double *inVector2, double *outVector);
double distance(double *position1, double *position2);
double vectorMagnitude(double *vector);
double dotProduct(double *vector1, double *vector2);
void reflectionVector(double *lightVector, double *normal, double *reflectionVector);


// Global Variables
int line  = 1;
int RECURSIONLEVEL = 1;
Scene scene;
int pixWidth;
int pixHeight;
Pixel *viewPlane;


int main(int c, char** argv) {
 printf("===== Begin Program =====\n");
 sscanf(argv[1], "%d", &pixWidth);
 sscanf(argv[2], "%d", &pixHeight);
 char *fileInput = argv[3];
 char *fileOutput = argv[4];
 //printf(" Testing %d %d %s %s\n", pixWidth, pixHeight, fileInput, fileOutput);
 scene.pixelWidth = pixWidth;
 scene.pixelHeight = pixHeight;
 //double temp[pixWidth][pixHeight];
 //viewPlane = temp;
 viewPlane = (Pixel *)malloc(pixWidth * pixHeight * sizeof(Pixel));
 read_scene(argv[3]);
 //printScene();
 raycast();

 //displayViewPlane();
 writePpmImage(fileOutput, 6);

 printf("===== End Program =====\n");
 return 0;
}

// next_c() wraps the getc() function and provides error checking and line
// number maintenance
int next_c(FILE* json) {
 int c = fgetc(json);
#ifdef DEBUG
 printf("next_c: '%c'\n", c);
#endif
 if (c == '\n') {
   line += 1;
 }
 if (c == EOF) {
   fprintf(stderr, "Error: Unexpected end of file on line number %d.\n", line);
   exit(1);
 }
 return c;
}

// expect_c() checks that the next character is d.  If it is not it emits
// an error.
void expect_c(FILE* json, int d) {
 int c = next_c(json);
 if (c == d) return;
 fprintf(stderr, "Error: Expected '%c' on line %d.\n", d, line);
 exit(1);
}

// skip_ws() skips white space in the file.
void skip_ws(FILE* json) {
 int c = next_c(json);
 while (isspace(c)) {
   c = next_c(json);
 }
 ungetc(c, json);
}

// next_string() gets the next string from the file handle and emits an error
// if a string can not be obtained.
char* next_string(FILE* json) {
 char buffer[129];
 int c = next_c(json);
 if (c != '"') {
   fprintf(stderr, "Error: Expected string on line %d.\n", line);
   exit(1);
 }
 c = next_c(json);
 int i = 0;
 while (c != '"') {
   if (i >= 128) {
     fprintf(stderr, "Error: Strings longer than 128 characters in length are not supported.\n");
     exit(1);
   }
   if (c == '\\') {
     fprintf(stderr, "Error: Strings with escape codes are not supported.\n");
     exit(1);
   }
   if (c < 32 || c > 126) {
     fprintf(stderr, "Error: Strings may contain only ascii characters.\n");
     exit(1);
   }
   buffer[i] = c;
   i += 1;
   c = next_c(json);
 }
 buffer[i] = 0;
 return strdup(buffer);
}

double next_number(FILE* json) {
 double value;
 fscanf(json, "%lf", &value);
 // Error check this..
 return value;
}

double* next_vector(FILE* json) {
 double* v = malloc(3*sizeof(double));
 expect_c(json, '[');
 skip_ws(json);
 v[0] = next_number(json);
 skip_ws(json);
 expect_c(json, ',');
 skip_ws(json);
 v[1] = next_number(json);
 skip_ws(json);
 expect_c(json, ',');
 skip_ws(json);
 v[2] = next_number(json);
 skip_ws(json);
 expect_c(json, ']');
 return v;
}

void read_scene(char* filename) {
 int c;
 FILE* json = fopen(filename, "r");

 if (json == NULL) {
   fprintf(stderr, "Error: Could not open file \"%s\"\n", filename);
   exit(1);
 }

 skip_ws(json);

 // Find the beginning of the list
 expect_c(json, '[');

 skip_ws(json);

 // Find the objects
 int objectIndex = 0;
 int lightIndex = 0;
 int genericIndex = 0;
 int isObject = -1;
 while (1) {
   c = fgetc(json);
   if (c == ']') {
     fprintf(stderr, "Error: This is the worst scene file EVER.\n");
     fclose(json);
     return;
   }
   if (c == '{') {
     skip_ws(json);

     // Parse the object
     char* key = next_string(json);
     if (strcmp(key, "type") != 0) {
       fprintf(stderr, "Error: Expected \"type\" key on line number %d.\n", line);
       exit(1);
     }

     skip_ws(json);

     expect_c(json, ':');

     skip_ws(json);

     char* value = next_string(json);

     if (strcmp(value, "camera") == 0) {
       // Do nothing, camera isn't an object in the scene.
     }
     else if (strcmp(value, "sphere") == 0) {
       scene.object[objectIndex].type = "sphere";
       genericIndex = objectIndex;
       objectIndex++;
       isObject = 1;
     }
     else if (strcmp(value, "plane") == 0) {
       scene.object[objectIndex].type = "plane";
       genericIndex = objectIndex;
       objectIndex++;
       isObject = 1;
     }
     else if (strcmp(value, "light") == 0) {
       scene.light[lightIndex].type = "pointlight";
       genericIndex = lightIndex;
       lightIndex++;
       isObject = 0;
     }
     else {
       fprintf(stderr, "Error: Unknown type, \"%s\", on line number %d.\n", value, line);
       exit(1);
     }

     skip_ws(json);

     while (1) {
     c = next_c(json);
     if (c == '}') {
       // stop parsing this object
       break;
     } else if (c == ',') {
     // read another field
     skip_ws(json);
     char* key = next_string(json);
     skip_ws(json);
     expect_c(json, ':');
     skip_ws(json);
     if (strcmp(key, "width") == 0) {
       scene.width = next_number(json);
     }
     else if (strcmp(key, "height") == 0) {
       scene.height = next_number(json);
     }
     else if (strcmp(key, "position") == 0) {
       if (isObject == 0) {
         scene.light[genericIndex].position = next_vector(json);
       }
       else if (isObject == 1) {
         scene.object[genericIndex].position = next_vector(json);
       }
     }
     else if (strcmp(key, "diffuse_color") == 0) {
       scene.object[genericIndex].diffuseColor = next_vector(json);
     }
     else if (strcmp(key, "specular_color") == 0) {
       scene.object[genericIndex].specularColor = next_vector(json);
     }
     else if (strcmp(key, "reflectivity") == 0) {
       scene.object[genericIndex].reflectivity = next_number(json);
     }
     else if (strcmp(key, "refractivity") == 0) {
       scene.object[genericIndex].refractivity = next_number(json);
     }
     else if (strcmp(key, "ior") == 0) {
       scene.object[genericIndex].ior = next_number(json);
     }
     // Sphere
     else if (strcmp(key, "radius") == 0) {
       scene.object[genericIndex].radius = next_number(json);
     }
     // Plane
     else if (strcmp(key, "normal") == 0) {
       scene.object[genericIndex].normal = next_vector(json);
     }
     // Lights
     else if (strcmp(key, "color") == 0) {
       scene.light[genericIndex].color = next_vector(json);
     }
     else if (strcmp(key, "direction") == 0) {
       scene.light[genericIndex].direction = next_vector(json);
       scene.light[genericIndex].type = "spotlight";
     }
     else if (strcmp(key, "radial-a2") == 0) {
       scene.light[genericIndex].radialA2 = next_number(json);
     }
     else if (strcmp(key, "radial-a1") == 0) {
       scene.light[genericIndex].radialA1 = next_number(json);
     }
     else if (strcmp(key, "radial-a0") == 0) {
       scene.light[genericIndex].radialA0 = next_number(json);
     }
     else if (strcmp(key, "angular-a0") == 0) {
       scene.light[genericIndex].angularA0 = next_number(json);
     }
     else if (strcmp(key, "theta") == 0) {
       scene.light[genericIndex].theta = next_number(json);
     }
     else {
       fprintf(stderr, "Error: Unknown property, \"%s\", on line %d.\n",
         key, line);
       //char* value = next_string(json);
     }
     skip_ws(json);
     } else {
       fprintf(stderr, "Error: Unexpected value on line %d\n", line);
       exit(1);
     }
     }
     skip_ws(json);
     c = next_c(json);
     if (c == ',') {
       // noop
       skip_ws(json);
     } else if (c == ']') {
       fclose(json);
       return;
     } else {
       fprintf(stderr, "Error: Expecting ',' or ']' on line %d.\n", line);
       exit(1);
     }
   }
 }
}

void printScene() {
 printf("\n===== Begin Printing Scene =====\n\n");

 printf("\tBegin Printing Objects:\n\n");

 int index = 0;
 while (scene.object[index].diffuseColor != NULL) {
   // Type
   if (scene.object[index].type != NULL) {
     printf("\t\tObject: %s\n", scene.object[index].type);
   }

   // Diffuse Color
   if (scene.object[index].diffuseColor != NULL) {
     printf("\t\t\tDiffuse Color: \t[%f, %f, %f]\n", scene.object[index].diffuseColor[0],
                                            scene.object[index].diffuseColor[1],
                                            scene.object[index].diffuseColor[2]);
   }

   // Specular Color
   if (scene.object[index].specularColor != NULL) {
     printf("\t\t\tSpecular Color: [%f, %f, %f]\n", scene.object[index].specularColor[0],
                                            scene.object[index].specularColor[1],
                                            scene.object[index].specularColor[2]);
   }

   // Position
   if (scene.object[index].position != NULL) {
     printf("\t\t\tPosition: \t[%f, %f, %f]\n", scene.object[index].position[0],
                                            scene.object[index].position[1],
                                            scene.object[index].position[2]);
   }

   // Normal
   if (scene.object[index].normal != NULL) {
     printf("\t\t\tNormal: \t[%f, %f, %f]\n", scene.object[index].normal[0],
                                          scene.object[index].normal[1],
                                          scene.object[index].normal[2]);
   }

   // Radius
   if (scene.object[index].radius != 0) {
     printf("\t\t\tRadius: \t%f\n", scene.object[index].radius);
   }

   printf("\t\tEnd Object: %s\n\n", scene.object[index].type);
   index ++;
 }
 index = 0;

 printf("\tEnd Printing Objects\n\n");

 printf("\tBegin Printing Lights:\n\n");

 while (scene.light[index].color != NULL) {
   // Type
   if (scene.light[index].type != NULL) {
     printf("\t\tLight: %s\n", scene.light[index].type);
   }

   // Color
   if (scene.light[index].color != NULL) {
     printf("\t\t\tColor: \t\t[%f, %f, %f]\n", scene.light[index].color[0],
                                           scene.light[index].color[1],
                                           scene.light[index].color[2]);
   }

   // Position
   if (scene.light[index].position != NULL) {
     printf("\t\t\tPosition: \t[%f, %f, %f]\n", scene.light[index].position[0],
                                            scene.light[index].position[1],
                                            scene.light[index].position[2]);
   }

   // Direction
   if (scene.light[index].direction != 0) {
     printf("\t\t\tDirection: \t[%f, %f, %f]\n", scene.light[index].direction[0],
                                             scene.light[index].direction[1],
                                             scene.light[index].direction[2]);
   }

   // Radial-a2
   if (scene.light[index].radialA2 != 0) {
     printf("\t\t\tRadial-a2: \t%f\n", scene.light[index].radialA2);
   }

   // Radial-a1
   if (scene.light[index].radialA1 != 0) {
     printf("\t\t\tRadial-a1: \t%f\n", scene.light[index].radialA1);
   }

   // Radial-a0
   if (scene.light[index].radialA0 != 0) {
     printf("\t\t\tRadial-a0: \t%f\n", scene.light[index].radialA0);
   }

   // Angular-a0
   if (scene.light[index].angularA0 != 0) {
     printf("\t\t\tAngular-a0: \t%f\n", scene.light[index].angularA0);
   }

   printf("\t\tEnd Light: %s\n\n", scene.light[index].type);
   index ++;
 }

 printf("\tEnd Printing Lights:\n\n");


 printf("===== End Printing Scene =====\n\n");
}

void raycast() {
 printf("\n===== Begin Raycasting =====\n\n");
 int row, column;
 double lookVector[3];
 double lookUVector[3];
 double vMagnitude;
 double origin[3] = {0, 0, 0};
 int pixelIndex = -1;
 // Loop through each pixel in the view plane
 for (row = scene.pixelHeight - 1; row >=0; row--) {
   for (column = 0; column < scene.pixelWidth; column++) {
     pixelIndex++;
     // Get the center of the Pixel i,j, get lookVector through pixel
     lookVector[0] = 0 - scene.width/2 + (scene.width/scene.pixelWidth)*(column + 0.5);
     lookVector[1] = 0 - scene.height/2 + (scene.height/scene.pixelHeight)*(row + 0.5);
     lookVector[2] = 1; // Looking down positive z axis

     // Get lookVector unit vector
     unitVector(lookVector, lookUVector);
     double startPosition[3];
     startPosition[0] = 0;
     startPosition[1] = 0;
     startPosition[2] = 0;
     Pixel finalColor;

     finalColor = shade(startPosition, lookVector, RECURSIONLEVEL);

     viewPlane[pixelIndex] = finalColor;
     printf("ViewPlane %d: [%f, %f, %f]\n", pixelIndex, viewPlane[pixelIndex].red, viewPlane[pixelIndex].green, viewPlane[pixelIndex].blue);
   }
 }
 printf("\n===== End Raycasting =====\n\n");
}

struct Pixel shade(double *startPosition, double *lookUVector, int recursionLevel) {
  printf("\n===== Begin Shading =====\n\n");
  //RecursionVariables
  double recursionPosition[3];
  double recursionLookUVector[3];
  Pixel returnColor;
  returnColor.red = 0;
  returnColor.green = 0;
  returnColor.blue = 0;
  //printf("Recursion Level %d\n", recursionLevel);
  if (recursionLevel == 0) {
    //printf("End Recursion\n\n");
    Pixel black;
    black.red = 0;
    black.green = 0;
    black.blue = 0;
    return black;
  }
  else {
    // Other Variables
     double vMagnitude;
     double origin[3] = {0, 0, 0};
     double cameraIntersection[3];
     double backgroundColor[3];
     backgroundColor[0] = 0.0;
     backgroundColor[1] = 0.0;
     backgroundColor[2] = 0.0;

     // Loop through objects in scene and solve for t
     int index = 0;
     double minT = -1;
     int objectIndexClosest = -1;
     double tClosestApproachMinusA;
     while (scene.object[index].diffuseColor != NULL) {
       // If object is sphere
       if (strcmp(scene.object[index].type,"sphere") == 0) {
         // Get distance of closest approach to sphere
         double t = tClosestApproachSphere(lookUVector, scene.object[index].position);

         // Use t to get the point at t from origin
         double tPosition[3];
         vectorMultiply(lookUVector, tPosition, t);

         // Solve for the distance between the closest approach and the center of the sphere
         double dist = distance(tPosition, scene.object[index].position);

         if (dist < scene.object[index].radius) {
           tClosestApproachMinusA = t - sqrt(pow(scene.object[index].radius,2) -
                                                 pow(dist,2));

           // Found a new closest object
           if (t > 0 && (minT <= 0 || tClosestApproachMinusA <= minT)) {
             minT = tClosestApproachMinusA;
             vectorMultiply(lookUVector, cameraIntersection, tClosestApproachMinusA);
             objectIndexClosest = index;
           }
         }
         index++;
       }
       else if (strcmp(scene.object[index].type,"plane") == 0) {
         //printf("Found a plane\n");
         // Get T Closest approach to plane
         double t = tClosestApproachPlane(scene.object[index].normal, origin, scene.object[index].position, lookUVector);
         if (t > 0 && (minT <= 0 || t <= minT)) {
           minT = t;
           vectorMultiply(lookUVector, cameraIntersection, t);
           objectIndexClosest = index;
         }
         index++;
       }
       else {
         // Not Plane or Sphere. Likely Light.
         printf("Error, not sphere or plane\n");
         index++;
       }
     }
     // If there was no intersection
     if (minT == -1) {
       Pixel tempPixel;
       tempPixel.red = 0;
       tempPixel.green = 0;
       tempPixel.red = 0;
       return tempPixel;
     }
     // If there was an intersection
     else {
       // Loop through Lights
       int lightIndex = 0;
       double lightVector[3];
       double lightUnitVector[3];
       double tClosest[3];
       double tClosestLight = -1;

       while (scene.light[lightIndex].color != NULL) {
         // Calculate distance from light to intersection
         double lightVector[3];
         vectorSubtract(cameraIntersection, scene.light[lightIndex].position, lightVector);

         // Calculate unit vector
         double lightUnitVector[3];
         unitVector(lightVector, lightUnitVector);
         // Calculate the magnitude of that vector, or the distance from the light to the intersection point;
         double lightVectorT = vectorMagnitude(lightVector);

         // Loop through objects and try to find a closer point to the light
         int shadowIndex = 0;
         int sentinel = 0; // 1 means shadow was found. 0 means no shadow was found
         int test = 0;
         while (sentinel == 0) {

           if (shadowIndex == objectIndexClosest) {
             shadowIndex++;
           }
           // Check again in case shadowIndex was incremented above.
           if (scene.object[shadowIndex].diffuseColor != NULL) {
             // check if vector intersects objects
             double t = 0;
             if (strcmp(scene.object[shadowIndex].type, "sphere") == 0) {

               // Calculate t of closest approach
               double shadowObjectPosition[3];
               vectorSubtract(scene.object[shadowIndex].position, scene.light[lightIndex].position, shadowObjectPosition);
               t = tClosestApproachSphere(lightUnitVector, shadowObjectPosition);

               // Calculate that point;
               double lightClosestPosition[3];
               vectorMultiply(lightUnitVector, lightClosestPosition, t);
               // Calculate distance between center of object and closest approach
               double dist = distance(lightClosestPosition, shadowObjectPosition);
               if (dist < scene.object[shadowIndex].radius) {
                 // Calculate a
                 double a = sqrt(pow(scene.object[shadowIndex].radius,2) - pow(dist,2));

                 // Calculate distance to intersection of light
                 t = t - a;
               }
               else {
                 t = -1;
               }
             }
             else if (strcmp(scene.object[shadowIndex].type, "plane") == 0) {
               double shadowObjectPosition[3];
               vectorSubtract(scene.object[shadowIndex].position, scene.light[lightIndex].position, shadowObjectPosition);
               t = tClosestApproachPlane(scene.object[shadowIndex].normal, origin, shadowObjectPosition, lightUnitVector);

             }
             // Check if closer to light than original
             if (t > 0 && t <= lightVectorT) {
               //printf("test\n");
               // Casts a shadow, Stop looking
               sentinel = 1;
             }
           }
           else {
             sentinel = 2;
           }
           shadowIndex++;
         }
         // There was no shadow, color it.
         if (sentinel == 2) {
           //printf("\nColoring location\n");
           // Calculate radial attenuation function
           double fRad = 1 / (scene.light[lightIndex].radialA2 * pow(lightVectorT,2) +
                              scene.light[lightIndex].radialA1 * lightVectorT +
                              scene.light[lightIndex].radialA0);

           //printf("fRad %f\n", fRad);

           // Calculate Diffuse Color Contribution
           double incidentDiffuse[3];
           double normal[3];
           // Calculate normal differently for spheres and planes
           if (strcmp(scene.object[objectIndexClosest].type, "sphere") == 0 ) {
             vectorSubtract(cameraIntersection, scene.object[objectIndexClosest].position, normal);
           }
           else if (strcmp(scene.object[objectIndexClosest].type, "plane") == 0 ) {
             normal[0] = scene.object[objectIndexClosest].normal[0];
             normal[1] = scene.object[objectIndexClosest].normal[1];
             normal[2] = scene.object[objectIndexClosest].normal[2];
           }
           unitVector(normal, normal);
           double dotDiffuse = -1 * dotProduct(lightUnitVector, normal);
           incidentDiffuse[0] = dotDiffuse * scene.light[lightIndex].color[0] * scene.object[objectIndexClosest].diffuseColor[0];
           incidentDiffuse[1] = dotDiffuse * scene.light[lightIndex].color[1] * scene.object[objectIndexClosest].diffuseColor[1];
           incidentDiffuse[2] = dotDiffuse * scene.light[lightIndex].color[2] * scene.object[objectIndexClosest].diffuseColor[2];
           //printf("Diffusion [%f. %f, %f]\n", incidentDiffuse[0], incidentDiffuse[1], incidentDiffuse[2]);

           // Calculate Specular Color Contribution
           double incidentSpecular[3];
           double reflectedVector[3];
           double surfaceToCamera[3];
           vectorMultiply(cameraIntersection, surfaceToCamera, -1);
           unitVector(surfaceToCamera, surfaceToCamera);
           reflectionVector(lightVector, normal, reflectedVector);
           unitVector(surfaceToCamera, surfaceToCamera);
           double vDotR = pow(dotProduct(reflectedVector,surfaceToCamera),50);
           if (vDotR < 0) {
             vDotR = 0;
           }

           // if spot light
             // if vodotvl < theta then 0
             //vdotvl^a1
           double fAng = 1;
           if (scene.light[lightIndex].theta != 0) {
             double vDotL = dotProduct(lightUnitVector, scene.light[lightIndex].direction);
             if (vDotL < scene.light[lightIndex].theta) {
               fAng = pow(vDotL, 1);
             }
             else {
               fAng = 0;
             }
           }
           //printf("fAng %f\n", fAng);

           incidentSpecular[0] = vDotR * scene.light[lightIndex].color[0] * scene.object[objectIndexClosest].specularColor[0];
           incidentSpecular[1] = vDotR * scene.light[lightIndex].color[1] * scene.object[objectIndexClosest].specularColor[1];
           incidentSpecular[2] = vDotR * scene.light[lightIndex].color[2] * scene.object[objectIndexClosest].specularColor[2];

           // Color that point
           returnColor.red   += fAng * fRad * (incidentDiffuse[0] + incidentSpecular[0]);
           returnColor.green += fAng * fRad * (incidentDiffuse[1] + incidentSpecular[1]);
           returnColor.blue  += fAng * fRad * (incidentDiffuse[2] + incidentSpecular[2]);
         }
         // There was a shadowing, do nothing.
         else {
           //printf("Not coloring pixel %d\n", pixelIndex);
           // Coloring ambient, temporary
         }
         lightIndex++;
       }
     }
   }
  shade(startPosition, lookUVector, recursionLevel - 1);
  return returnColor;
  printf("\n===== End Shading =====\n\n");
}

void displayViewPlane() {
 printf("\n===== Begin Scene Display =====\n\n");
 int row, column;
 int topRow;
 for (topRow = 0; topRow < scene.pixelHeight * 2 + 1; topRow++) {
   printf("#");
 }
 printf("\n");
 int pixelIndex = -1;
 for (row = 0; row < scene.pixelHeight; row++) {
   printf("# ");
   for (column = 0; column < scene.pixelWidth; column++) {
     pixelIndex++;
     /*printf("PixelColor(%d, %d) = (%f, %f, %f)\n", row, column,
                             viewPlane[row * scene.pixelHeight + column][0],
                             viewPlane[row * scene.pixelHeight + column][1],
                             viewPlane[row * scene.pixelHeight + column][2]);*/
     if (viewPlane[pixelIndex].red == 1) {
       printf("R ");
     }
     else if (viewPlane[pixelIndex].green == 1) {
       printf("/ ");
     }
     else if (viewPlane[pixelIndex].blue == 1) {
       printf(". ");
     }
     else {
       printf("  ");
     }
   }
   printf("\n");
 }
 printf("\n===== End Scene Display =====\n\n");
}

void writePpmImage(char *outFilename, int format) {
 printf("\n===== Begin Writing File =====\n\n");
 FILE *outFile = fopen(outFilename, "wb");
 //Write header
 if (format == 6) {
   fwrite("P6\n", 3, 1, outFile);
 }
 else if (format == 3) {
   fwrite("P3\n", 3, 1, outFile);
 }
 fwrite("# Testing Output\n", 17, 1, outFile);
 char buffer[9];
 sprintf(buffer,"%d",pixWidth);
 fwrite(buffer, strlen(buffer), 1, outFile);
 fwrite(" ", 1, 1, outFile);
 sprintf(buffer,"%d", pixHeight);
 fwrite(buffer, strlen(buffer), 1, outFile);
 fwrite("\n", 1, 1, outFile);
 sprintf(buffer, "%d", 255);
 fwrite(buffer, strlen(buffer), 1, outFile);
 fwrite("\n", 1, 1, outFile);

 //Write data
 int index;
 if (format == 6) {
   for (index = 0; index < pixWidth * pixHeight; index++) {
     // get color * 255 floored
     int color = (int) (viewPlane[index].red * 255);
     if (color < 0){
       color = 0;
     }
     if (color > 255){
       color = 255;
     }
     sprintf(buffer, "%c", color);
     fwrite(buffer, 1, 1, outFile);
     color = (int) (viewPlane[index].green * 255);
     if (color < 0){
       color = 0;
     }
     if (color > 255){
       color = 255;
     }
     sprintf(buffer, "%c", color);
     fwrite(buffer, 1, 1, outFile);
     color = (int) (viewPlane[index].blue* 255);
     if (color < 0){
       color = 0;
     }
     if (color > 255){
       color = 255;
     }
     sprintf(buffer, "%c", color);
     fwrite(buffer, 1, 1, outFile);
   }
 }
 else if (format == 3) {
   for (index = 0; index < pixWidth * pixHeight; index++) {
     // get color * 255 floored
     int color = (int) (viewPlane[index].red * 255);
     if (color < 0){
       color = 0;
     }
     if (color > 255){
       color = 255;
     }
     sprintf(buffer, "%d", color);
     fwrite(buffer, strlen(buffer), 1, outFile);
     fwrite("\n", 1, 1, outFile);
     color = (int) (viewPlane[index].green * 255);
     if (color < 0){
       color = 0;
     }
     if (color > 255){
       color = 255;
     }
     sprintf(buffer, "%d", color);
     fwrite(buffer, strlen(buffer), 1, outFile);
     fwrite("\n", 1, 1, outFile);
     color = (int) (viewPlane[index].blue * 255);
     if (color < 0){
       color = 0;
     }
     if (color > 255){
       color = 255;
     }
     sprintf(buffer, "%d", color);
     fwrite(buffer, strlen(buffer), 1, outFile);
     fwrite("\n", 1, 1, outFile);
   }
 }
 fclose(outFile);
 printf("\n===== End Writing File =====\n\n");
}

void unitVector(double *vector, double *unitVector) {
 double vMagnitude = sqrt(pow(vector[0],2) + pow(vector[1],2) + pow(vector[2],2));
 unitVector[0] = vector[0] / vMagnitude;
 unitVector[1] = vector[1] / vMagnitude;
 unitVector[2] = vector[2] / vMagnitude;
}

double tClosestApproachSphere(double *vector, double *position) {
 return (vector[0] * position[0] +
         vector[1] * position[1] +
         vector[2] * position[2]) /
     (pow(vector[0],2) + pow(vector[1],2) + pow(vector[2],2));

}

double tClosestApproachPlane(double *normal, double *origin, double *position, double *lookUVector) {
 return -(normal[0] * (origin[0] - position[0]) +
      normal[1] * (origin[1] - position[1]) +
      normal[2] * (origin[2] - position[2])) /
      (normal[0] * lookUVector[0] +
       normal[1] * lookUVector[1] +
       normal[2] * lookUVector[2]);
}

void vectorMultiply(double *inVector, double *outVector, double mult) {
 outVector[0] = inVector[0] * mult;
 outVector[1] = inVector[1] * mult;
 outVector[2] = inVector[2] * mult;
}

void vectorDivide(double *inVector, double *outVector, double divide) {
 outVector[0] = inVector[0] / divide;
 outVector[1] = inVector[1] / divide;
 outVector[2] = inVector[2] / divide;
}


void vectorSubtract(double *inVector1, double *inVector2, double *outVector) {
 outVector[0] = inVector1[0] - inVector2[0];
 outVector[1] = inVector1[1] - inVector2[1];
 outVector[2] = inVector1[2] - inVector2[2];
}

void vectorAddition(double *inVector1, double *inVector2, double *outVector) {
 outVector[0] = inVector1[0] + inVector2[0];
 outVector[1] = inVector1[1] + inVector2[1];
 outVector[2] = inVector1[2] + inVector2[2];
}

double distance(double *position1, double *position2) {
 return sqrt(pow(position1[0] - position2[0], 2) +
             pow(position1[1] - position2[1], 2) +
             pow(position1[2] - position2[2], 2));
}

double vectorMagnitude(double *vector) {
 return sqrt(pow(vector[0], 2) +
             pow(vector[1], 2) +
             pow(vector[2], 2));
}

double dotProduct(double *vector1, double *vector2) {
 return (vector1[0] * vector2[0] +
         vector1[1] * vector2[1] +
         vector1[2] * vector2[2]);
}

void reflectionVector(double *lightVector, double *normal, double *reflectionVector) {
 unitVector(lightVector, lightVector);
 unitVector(normal,normal);
 double dotProd = dotProduct(normal, lightVector);
 if (dotProd < 0) {
   dotProd *= 2;
   vectorMultiply(normal, reflectionVector, dotProd);
   vectorSubtract(lightVector, reflectionVector, reflectionVector);
   unitVector(reflectionVector, reflectionVector);
 }
 else {
   vectorMultiply(normal, reflectionVector, 0);
 }
}
