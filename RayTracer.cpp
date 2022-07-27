#include <iostream>
using namespace std;
#include <cmath>
#include <vector>
#include <glm/glm.hpp>
#include "Sphere.h"
#include "SceneObject.h"
#include "Ray.h"
#include "Plane.h"
#include "TextureBMP.h"
#include <GL/freeglut.h>


const float WIDTH = 20.0;  
const float HEIGHT = 20.0;
const float EDIST = 40.0;
const int NUMDIV = 500;
const int MAX_STEPS = 5;
const float XMIN = -WIDTH * 0.5;
const float XMAX =  WIDTH * 0.5;
const float YMIN = -HEIGHT * 0.5;
const float YMAX =  HEIGHT * 0.5;
const bool antiAliasing = false;
const bool bugShadow = true;
const bool tablePattern = true;
const bool showRefrac = true;
const bool fog = false;

vector<SceneObject*> sceneObjects;
TextureBMP texture;
TextureBMP wallTexture;

//----------------------------------------------------------------------------------
//   Computes the colour value obtained by tracing a ray and finding its 
//     closest point of intersection with objects in the scene.
//----------------------------------------------------------------------------------
glm::vec3 trace(Ray ray, int step)
{
	glm::vec3 backgroundCol(0,.7,1);						
	glm::vec3 lightPos(-20, 200, -75);					
	glm::vec3 color(0);
	SceneObject* obj;

    ray.closestPt(sceneObjects);					
    if(ray.index == -1) return backgroundCol;		
	obj = sceneObjects[ray.index];					

	//Ground Stripe
	if (ray.index == 0)
	{
		//Stripe pattern
		int stripeWidth = 5;
		int iz = ((ray.hit.z) + 500) / stripeWidth;
		int ix = ((ray.hit.x) + 500) / stripeWidth;
		int k = int(iz) % 2; //2 colors
		int j = int(ix) % 2;
		if (k + j == 1) color = glm::vec3(0, 1, 0); //Green
		else color = glm::vec3(1, 0.8, 0.7); //Baige
		obj->setColor(color);

	}
	//Wall Texture
	if (ray.index == 1) {
		float texcoords = (ray.hit.x + 100) / (100 - -100);
		float texcoordt = (ray.hit.y + 15) / (100 - -15);
		if (texcoords > 0 && texcoords < 1 && texcoordt > 0 && texcoordt < 1)
		{
			color = wallTexture.getColorAt(texcoords, texcoordt);
			obj->setColor(color);
		}
	}
	//Table Top Texture
	if (ray.index == 2) {
		float texcoords = (ray.hit.x + 23) / (23 - -23);
		float texcoordt = (ray.hit.z + 50) / (-100 + 50);
		if (texcoords > 0 && texcoords < 1 && texcoordt > 0 && texcoordt < 1)
		{
			color = texture.getColorAt(texcoords, texcoordt);
			obj->setColor(color);
		}
	}
	//Table side Texture
	if (ray.index == 3 && tablePattern) {
		float size = (1.0/3);
		int x = ((ray.hit.x) + 501) / size;
		int y = ((ray.hit.y) + 501) / size;
		int i = int(x) % 5;
		int j = int(y) % 6;
		if (i+j == 0 || i+j==5 || i*j == 2 || i * j == 3 || i * j == 8 || i * j == 12) color = glm::vec3(1, 0, 0);
		else color = glm::vec3(1, 1, 1);
		obj->setColor(color);
	} 
	else if (ray.index == 3) {
		float texcoords = (ray.hit.x + 23) / (23 - -23);
		float texcoordt = (ray.hit.y + 7) / (-9 + 7);
		if (texcoords > 0 && texcoords < 1 && texcoordt > 0 && texcoordt < 1)
		{
			color = texture.getColorAt(texcoords, texcoordt);
			obj->setColor(color);
		}
	}

	color = obj->lighting(lightPos, -ray.dir, ray.hit);
	
	//Fog Logic
	if (fog && step < MAX_STEPS) {
		int z1 = -55.0;
		int z2 = -300.0;
		float t = ((ray.hit.z) - z1) / (z2 - z1);
		glm::vec3 white(1, 1, 1);
		color = ((1 - t) * color) + (t * white);
	}

	glm::vec3 lightVec = lightPos - ray.hit;
	Ray shadowRay(ray.hit, lightVec);
	shadowRay.closestPt(sceneObjects);
	SceneObject* obj2;
	if (shadowRay.index > -1 && shadowRay.dist < glm::length(lightVec)) {
		obj2 = sceneObjects[shadowRay.index];
		if (ray.index == 9 && bugShadow) {
		} else if (obj2->isTransparent() || obj2->isRefractive()) {
			color = 0.6f * obj->getColor();
		} else {
			color = 0.2f * obj->getColor(); //0.2 = ambient scale factor
		}
	}

	//Refraction Logic
	if (obj->isRefractive() && showRefrac && step < MAX_STEPS) {
		float eta = obj->getRefractiveIndex();
		float rho = obj->getRefractionCoeff();
		glm::vec3 n = obj->normal(ray.hit);
		glm::vec3 g = glm::refract(ray.dir, n, eta);
		Ray refrRay(ray.hit, g);
		refrRay.closestPt(sceneObjects);
		glm::vec3 m = obj->normal(refrRay.hit);
		glm::vec3 h = glm::refract(g, -m, 1.0f / eta);
		Ray refractedRay(ray.hit, h);
		glm::vec3 refractedColor = trace(refractedRay, step + 1);
		color = (obj->getRefractionCoeff() * refractedColor);
	}

	//Reflection Logic
	if (obj->isReflective() && step < MAX_STEPS)
	{
		float rho = obj->getReflectionCoeff();
		glm::vec3 normalVec = obj->normal(ray.hit);
		glm::vec3 reflectedDir = glm::reflect(ray.dir, normalVec);
		Ray reflectedRay(ray.hit, reflectedDir);
		glm::vec3 reflectedColor = trace(reflectedRay, step + 1);
		color = color + (rho * reflectedColor);
	}

	//Transparency Logic
	if (obj->isTransparent() && step < MAX_STEPS)
	{
		float rho = obj->getTransparencyCoeff();
		Ray tranRay(ray.hit, ray.dir);
		glm::vec3 transparentColor = trace(tranRay, step + 1);
		color = color + (rho * transparentColor);
	}


	return color;
}



//---The main display module -----------------------------------------------------------
// In a ray tracing application, it just displays the ray traced image by drawing
// each cell as a quad.
//---------------------------------------------------------------------------------------
void display()
{
	float xp, yp;  //grid point
	float cellX = (XMAX-XMIN)/NUMDIV;  //cell width
	float cellY = (YMAX-YMIN)/NUMDIV;  //cell height
	glm::vec3 eye(0., 0., 0.);

	glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

	glBegin(GL_QUADS);  //Each cell is a tiny quad.

	glm::vec3 prevCol(0);
	for(int i = 0; i < NUMDIV; i++)	//Scan every cell of the image plane
	{
		xp = XMIN + i*cellX;
		
		for(int j = 0; j < NUMDIV; j++)
		{
			yp = YMIN + j*cellY;

		    glm::vec3 dir(xp+0.5*cellX, yp+0.5*cellY, -EDIST);	//direction of the primary ray

		    Ray ray = Ray(eye, dir);

		    glm::vec3 col = trace (ray, 1); //Trace the primary ray and get the colour value

			glColor3f(col.r, col.g, col.b);
			glVertex2f(xp, yp);				//Draw each cell with its color value
			glVertex2f(xp + cellX, yp);
			glVertex2f(xp + cellX, yp + cellY);
			glVertex2f(xp, yp + cellY);

			//antiAliasing
			if (abs(col.r - prevCol.r) > 0.2 || abs(col.g - prevCol.g) > 0.2 || abs(col.b - prevCol.b) > 0.2 && antiAliasing) {
				glm::vec3 dir1(xp + 0.25 * cellX, yp + 0.25 * cellY, -EDIST);	
				Ray ray1 = Ray(eye, dir1);
				glm::vec3 col1 = trace(ray1, 1); 

				glm::vec3 dir2(xp + 0.75 * cellX, yp + 0.25 * cellY, -EDIST);	
				Ray ray2 = Ray(eye, dir2);
				glm::vec3 col2 = trace(ray2, 1); 

				glm::vec3 dir3(xp + 0.75 * cellX, yp + 0.75 * cellY, -EDIST);	
				Ray ray3 = Ray(eye, dir3);
				glm::vec3 col3 = trace(ray3, 1); 

				glm::vec3 dir4(xp + 0.25 * cellX, yp + 0.75 * cellY, -EDIST);	
				Ray ray4 = Ray(eye, dir4);
				glm::vec3 col4 = trace(ray4, 1);

				float r = ((float)col1.r / 4.0 + (float)col2.r / 4.0 + (float)col3.r / 4.0 + (float)col4.r / 4.0);
				float g = ((float)col1.g / 4.0 + (float)col2.g / 4.0 + (float)col3.g / 4.0 + (float)col4.g / 4.0);
				float b = ((float)col1.b / 4.0 + (float)col2.b / 4.0 + (float)col3.b / 4.0 + (float)col4.b / 4.0);

				glColor3f(r, g, b);
				glVertex2f(xp, yp);				//Draw each cell with its color value
				glVertex2f(xp + cellX, yp);
				glVertex2f(xp + cellX, yp + cellY);
				glVertex2f(xp, yp + cellY);
			}
			
			prevCol = col;
        }
    }

    glEnd();
    glFlush();
}


//---This function initializes the scene ------------------------------------------- 
//   Specifically, it creates scene objects (spheres, planes, cones, cylinders etc)
//     and add them to the list of scene objects.
//   It also initializes the OpenGL orthographc projection matrix for drawing the
//     the ray traced image.
//----------------------------------------------------------------------------------
void initialize()
{
	texture = TextureBMP("Wood.bmp");
	wallTexture = TextureBMP("WallTexture.bmp");
    glMatrixMode(GL_PROJECTION);
    gluOrtho2D(XMIN, XMAX, YMIN, YMAX);

    glClearColor(0, 0, 0, 1);
	//Floor
	Plane* plane1 = new Plane(
		glm::vec3(-100., -15, -40), //Point A
		glm::vec3(100., -15, -40), //Point B
		glm::vec3(100., -15, -250), //Point C
		glm::vec3(-100., -15, -250)); //Point D
	plane1->setColor(glm::vec3(0.8, 0.8, 0));
	plane1->setReflectivity(true, 0.1);
	sceneObjects.push_back(plane1);

	//Wall
	Plane* wall = new Plane(
		glm::vec3(-100., -15, -250), //Point A
		glm::vec3(100., -15, -250), //Point B
		glm::vec3(100., 70, -250), //Point C
		glm::vec3(-100., 70, -250)); //Point D
	wall->setColor(glm::vec3(0.8, 0.8, 0));
	sceneObjects.push_back(wall);

	//Table
	Plane* plane2 = new Plane(
		glm::vec3(-23, -7, -50), //Point A
		glm::vec3(23, -7, -50), //Point B
		glm::vec3(23, -7,-100), //Point C
		glm::vec3(-23, -7, -100)); //Point D
	plane2->setColor(glm::vec3(0.8, 0.8, 0));
	plane2->setReflectivity(true, 0.5);
	plane2->setShininess(50);
	sceneObjects.push_back(plane2);

	Plane* plane3 = new Plane(
		glm::vec3(-23, -7, -50), //Point A
		glm::vec3(23, -7, -50), //Point B
		glm::vec3(23, -9, -50), //Point C
		glm::vec3(-23, -9, -50)); //Point D
	plane3->setColor(glm::vec3(0.8, 0.8, 0));
	plane3->setShininess(50);
	sceneObjects.push_back(plane3);

	//Pyramid1
	Plane* pyra11 = new Plane(
		glm::vec3(-7, -7, -75), //Point A
		glm::vec3(-12, -7, -75), //Point B
		glm::vec3(-9.5, -3.25, -72.835)); //Point C
	pyra11->setColor(glm::vec3(1, 1, 0));
	pyra11->setShininess(50);
	sceneObjects.push_back(pyra11);

	Plane* pyra12 = new Plane(
		glm::vec3(-7, -7, -75), //Point A
		glm::vec3(-9.5, -7, -70.67), //Point B
		glm::vec3(-9.5, -3.25, -72.835)); //Point C
	pyra12->setColor(glm::vec3(1, 1, 0));
	pyra12->setShininess(50);
	sceneObjects.push_back(pyra12);

	Plane* pyra13 = new Plane(
		glm::vec3(-12, -7, -75), //Point A
		glm::vec3(-9.5, -7, -70.67), //Point B
		glm::vec3(-9.5, -3.25, -72.835)); //Point C
	pyra13->setColor(glm::vec3(1, 1, 0));
	pyra13->setShininess(50);
	sceneObjects.push_back(pyra13);
	//

	Sphere *sphere1 = new Sphere(glm::vec3(0, -5, -90.0), 5.0);
	sphere1->setColor(glm::vec3(1, 0.5, 0.5));   //Set colour to blue
	sphere1->setReflectivity(true, 0.3);
	sceneObjects.push_back(sphere1);		 //Add sphere to scene objects

	Sphere* sphere2 = new Sphere(glm::vec3(-9.5, 0.75, -72.835), 4.0);
	sphere2->setColor(glm::vec3(0.8, 0.8, 0.8));   //Set colour to blue
	sphere2->setTransparency(true, 0.3);
	sphere2->setRefractivity(true, 0.6, 1.0/1.1);
	sphere2->setReflectivity(true, 0.2);
	sceneObjects.push_back(sphere2);		 //Add sphere to scene objects

	Sphere* sphere3 = new Sphere(glm::vec3(10.5, -3, -65.835), 3.0);
	sphere3->setColor(glm::vec3(0.8, 0.8, 0.2));   //Set colour to blue
	sphere3->setTransparency(true, 0.7);
	sphere3->setReflectivity(true, 0.1);
	sceneObjects.push_back(sphere3);		 //Add sphere to scene objects

	Plane* board = new Plane(
		glm::vec3(-20, 5.5, -100), //Point A
		glm::vec3(0, 5.5, -100),
		glm::vec3(0, -4.5, -100), //Point D
		glm::vec3(-20, -4.5, -100)); //Point B
	board->setColor(glm::vec3(1, 1, 0));
	board->setShininess(50);
	//sceneObjects.push_back(board);

	Plane* plane4 = new Plane(
		glm::vec3(-23, -9, -50), //Point A
		glm::vec3(23, -9, -50), //Point B
		glm::vec3(23, -11, -100), //Point C
		glm::vec3(-23, -11, -100)); //Point D
	plane4->setColor(glm::vec3(0.8, 0.8, 0));
	sceneObjects.push_back(plane4);
}


int main(int argc, char *argv[]) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_SINGLE | GLUT_RGB );
    glutInitWindowSize(500, 500);
    glutInitWindowPosition(20, 20);
    glutCreateWindow("Raytracing");

    glutDisplayFunc(display);
    initialize();

    glutMainLoop();
    return 0;
}
