#include <CanvasTriangle.h>
#include <DrawingWindow.h>
#include <Utils.h>
#include <CanvasPoint.h>
#include <CanvasTriangle.h>
#include <Colour.h>
#include <TextureMap.h>
#include <ModelTriangle.h>
#include <RayTriangleIntersection.h>
#include <iostream>
#include <thread>
#include <fstream>
#include <random>
#include <cmath>
#include <tuple>
#include <vector>
#include <set>
#include <glm/glm.hpp>
#include <cstring>  // included for memset, used to reset depthArray
#include <unordered_map> //added for new obj parser

#define pi 3.1415926535
#define WIDTH 320*3
#define HEIGHT 240*3
float DepthArray [HEIGHT] [WIDTH] = {{0}};
glm::vec3 cameraPosition (0.0, 0.0, 4.0);
glm::mat3 cameraOrientation ({1,0,0},{0,1,0},{0,0,1});
enum renderType{WIREFRAME, RASTERISE, RAYTRACE};
renderType renderMode = WIREFRAME;
std::array<bool, 3> lightingMode = {0,0,0};
bool USE_MIRROR = 0;
bool METALIC_MIRROR = 0;
bool USE_TEXTURED_FLOOR = 1;
bool USE_SKYBOX = 1;
bool USE_NORMAL_MAP = 1;
bool GDB_FILES = 0;
//0-proximity, 1-aoi, 2-specular Lighting

uint32_t convertColour(const Colour& colour) {
    return (static_cast<uint32_t>(colour.red) << 16) | 
           (static_cast<uint32_t>(colour.green) << 8) | 
           (static_cast<uint32_t>(colour.blue));
}

std::tuple<std::vector<Colour>, std::vector<std::string>, std::vector<std::string>> MTLparser (std::string fileLocation){
	//convert file to a vector of lines

	std::string line;
	std::vector<std::string> lines;
	std::ifstream File(fileLocation);

	while (getline (File, line)) {
		lines.push_back(line);
	}

	std::vector<Colour> ColourValues;
	std::vector<std::string> ColourNames;
	std::vector<std::string> TextureFiles;

    for (size_t i = 0; i < lines.size(); i += 3) {
		//adds all the colour names to a vector
		std::vector<std::string> head = split(lines[i], ' ');
		ColourNames.push_back(head[1]);

		//extracts colour values to colourLine as a string
		std::vector<std::string> colourLine = split(lines[i+1], ' ');
		colourLine.erase(colourLine.begin());

		//converts colourLine values to floats * 255 to remove standardised form
		std::vector<float> ColourRGB;
		for (std::string item : colourLine) {
			ColourRGB.push_back(std::stof(item)*255);
			//W: changed from stof
		}

		//adds Colour models to ColourValues
		ColourValues.push_back(Colour(ColourRGB[0],ColourRGB[1],ColourRGB[2]));

		if (!lines[i+2].empty()){
			TextureFiles.push_back(split(lines[i+2], ' ')[1]);
			i++; //extra i++ for if theres the extra texutre line
		} else{
			TextureFiles.push_back("");
		}
	}

    return std::make_tuple(ColourValues, ColourNames, TextureFiles);
}

std::vector<ModelTriangle> OBJparser (std::string fileLocation, std::tuple<std::vector<Colour>, std::vector<std::string>, std::vector<std::string>> colours, float scaler, glm::vec3 offset){
    std::vector<Colour> colourValues = std::get<0>(colours);
    std::vector<std::string> colourNames = std::get<1>(colours);

    std::string line;
    std::vector<std::string> lines;
    std::ifstream File(fileLocation);

    while (getline(File, line)) {
        lines.push_back(line);
    }

    std::vector<glm::vec3> vertices;
    std::vector<ModelTriangle> triangles;
    std::unordered_map<int, glm::vec3> vertexNormals; //stores accumulated normals for each vertex index
	std::vector<TexturePoint> texturePoints;
    Colour colour;

    for (std::string l : lines) {
        if (l.substr(0, 6) == "usemtl") {
            std::string colourName = l.substr(7);
            size_t index = 0;

            while (colourNames[index] != colourName) {
				if (index>=colourNames.size()){
					std::cout<<"colour name not found"<<std::endl;
					std::cout<<colourName<<std::endl;
					break;
				}
                index++;
            }
            colour = colourValues[index];

        } else if (l.substr(0, 2) == "v ") {
            std::vector<std::string> coords = split(l.substr(2), ' ');
            glm::vec3 vertex(std::stof(coords[0]), std::stof(coords[1]), std::stof(coords[2]));
			//apply scaler
			vertex[0] = vertex[0] * scaler;
			vertex[1] = vertex[1] * scaler;
			vertex[2] = vertex[2] * scaler;

			//apply offset
			vertex[0] = vertex[0] + offset.x;
			vertex[1] = vertex[1] + offset.y;
			vertex[2] = vertex[2] + offset.z;

            vertices.push_back(vertex);
        } else if (l.substr(0,2) == "vt"){ 
            std::vector<std::string> texturePointsRaw = split(l.substr(2), ' ');
			TexturePoint tp(std::stof(texturePointsRaw[1]),std::stof(texturePointsRaw[2]));
			texturePoints.push_back(tp);
		} else if (l.substr(0, 2) == "f ") {
			//splits line into the three triangle indexes
            std::vector<std::string> triangleVerticesIndex = split(l.substr(2), ' ');
			//grabs the potential spot for a texture point index
			//if it is empty, there is no texture point here.
			std::string tpIndex0 = split(triangleVerticesIndex[0],'/')[1];
			std::string tpIndex1 = split(triangleVerticesIndex[1],'/')[1];
			std::string tpIndex2 = split(triangleVerticesIndex[2],'/')[1];

            std::vector<size_t> triangleVerticesIntIndex;
            for (const std::string& item : triangleVerticesIndex) {
                int itemInt = std::stoi(item) - 1;
                triangleVerticesIntIndex.push_back(itemInt);
            }

			//assembles triangles
            ModelTriangle triangle(
                vertices[triangleVerticesIntIndex[0]],
                vertices[triangleVerticesIntIndex[1]],
                vertices[triangleVerticesIntIndex[2]],
                colour
            );

			//if there is a texture points, grab the corresponding one and put it in triangle.texturePoints
			if (!tpIndex0.empty()){
				triangle.texturePoints[0] = texturePoints[std::stoi(tpIndex0)-1];
				triangle.texturePoints[1] = texturePoints[std::stoi(tpIndex1)-1];
				triangle.texturePoints[2] = texturePoints[std::stoi(tpIndex2)-1];

			}

			std::array<int, 3> indexes = { static_cast<int>(triangleVerticesIntIndex[0]),
                                static_cast<int>(triangleVerticesIntIndex[1]),
                                static_cast<int>(triangleVerticesIntIndex[2]) };			
			
			triangle.vertexIndex = indexes;

            glm::vec3 normal = glm::normalize(
                glm::cross(
                    vertices[triangleVerticesIntIndex[2]] - vertices[triangleVerticesIntIndex[1]],
                    vertices[triangleVerticesIntIndex[0]] - vertices[triangleVerticesIntIndex[1]]
                )
            );

            triangle.normal = normal;
			// triangle.name = fileName;
            triangles.push_back(triangle);

            //adds normal to corresponding index of each vertex used
			for (size_t index : triangleVerticesIntIndex) {
				vertexNormals[index] = vertexNormals[index] + normal;
			}
        }
    }

    //normalise accumulated normals, and assign to each vertex
    for (int j=0; j<triangles.size(); j++) {
        for (int i = 0; i < 3; ++i) {
            int vertexIndex = triangles[j].vertexIndex[i];
            triangles[j].normals[i] = glm::normalize(vertexNormals[vertexIndex]);
        }
    }

	std::cout<<"obj parser done"<<std::endl;

    return triangles;
}

//this is intended to work from the camera
RayTriangleIntersection getClosestIntersection (glm::vec3 rayDirection, std::vector<ModelTriangle> &triangles, glm::vec3 r){
	//r is ray source, should not be hard coded to camera position
	// glm::vec3 r = cameraPosition;
	float minT = FLT_MAX;
	ModelTriangle closestTriangle;
	size_t triangleIndex;
	bool intersectionFound = false;
    glm::vec3 intersectionPoint = glm::vec3(0);


	for (int i=0; i<triangles.size(); i++){
		ModelTriangle triangle=triangles[i];
		glm::vec3 e0 = triangle.vertices[1] - triangle.vertices[0];
		glm::vec3 e1 = triangle.vertices[2] - triangle.vertices[0];
		glm::vec3 SPVector = r - triangle.vertices[0];
		glm::mat3 DEMatrix(-rayDirection, e0, e1);
		glm::vec3 possibleSolution = glm::inverse(DEMatrix) * SPVector;

		if (possibleSolution[1]>=0 && possibleSolution[2]>=0
		&& possibleSolution[1]<=1 && possibleSolution[2]<=1
		&& possibleSolution[1]+possibleSolution[2]<=1
		&& possibleSolution[0]>0){
			intersectionFound=true;

			//intersection is closer!
			if (possibleSolution[0]<minT){
				minT=possibleSolution[0];
				closestTriangle=triangle;
				triangleIndex=i;
				intersectionPoint = triangle.vertices[0] + possibleSolution[1]*e0 + possibleSolution[2]*e1;
			}
		}
	}

	if (!intersectionFound){
		return RayTriangleIntersection(glm::vec3(0), -1, ModelTriangle(), 0);
	}

	RayTriangleIntersection intersectionDetails (intersectionPoint, minT, closestTriangle, triangleIndex);
	return intersectionDetails;
}

bool findIntersectionBetweenTwoPoints(glm::vec3 source, glm::vec3 destination, std::vector<ModelTriangle> &triangles) {
    glm::vec3 rayDirection = glm::normalize(destination - source);
    float rayDistance = glm::length(destination - source);
    bool intersectionFound = false;

	source = source + 0.01f * rayDirection;

    for (int i = 0; i < triangles.size(); i++) {
        ModelTriangle triangle = triangles[i];
        glm::vec3 e0 = triangle.vertices[1] - triangle.vertices[0];
        glm::vec3 e1 = triangle.vertices[2] - triangle.vertices[0];
        glm::vec3 SPVector = source - triangle.vertices[0];
        glm::mat3 DEMatrix(-rayDirection, e0, e1);
        glm::vec3 possibleSolution = glm::inverse(DEMatrix) * SPVector;

        //check if intersection
        if (possibleSolution[1] >= 0 && possibleSolution[2] >= 0 &&
            possibleSolution[1] <= 1 && possibleSolution[2] <= 1 &&
            possibleSolution[1] + possibleSolution[2] <= 1 &&
            possibleSolution[0] > 0.01 && possibleSolution[0] < rayDistance) {//check intersection before the destination
            intersectionFound = true;
            break; //break if intersection fround
        }
    }

    return intersectionFound;
}

CanvasPoint projectVertexOntoCanvasPoint(glm::vec3 cameraPosition, float focalLength, glm::vec3 vertexPosition){
	size_t scaler = WIDTH/2;

	//converting from model coordingate system to camera coordinate system
	//getting vertex position relative to the camera location
	glm::vec3 relativeVertexPosition = vertexPosition - cameraPosition;

	//rotating it by the camera Orientation matrix
	glm::vec3 rotatedVertexPosition = cameraOrientation * relativeVertexPosition;

	float u = -(focalLength * rotatedVertexPosition.x) / rotatedVertexPosition.z;
    float v = (focalLength * rotatedVertexPosition.y) / rotatedVertexPosition.z;

	u = u *scaler;
	v = v *scaler;

	u = u + WIDTH/2;
	v = v + HEIGHT/2;

	CanvasPoint result (u,v);

	result.depth = 1/rotatedVertexPosition.z;

	return result;
}

void sortByY(std::vector<CanvasPoint>& points) {
	if (points[1].y < points[0].y){
		std::swap(points[1],points[0]);
	}
	if (points[2].y < points[1].y){
		std::swap(points[2],points[1]);
	}
	if (points[1].y < points[0].y){
		std::swap(points[1],points[0]);
	}
}

//cant handle negative values!!

std::vector<CanvasPoint> lineValuesWithDepth(CanvasPoint from, CanvasPoint to){

	if (!from.depth || !to.depth){
		from.depth = 0;
		to.depth = 0;
		//absurdly janky solution :/
		//seems to cause a memory leak issue at a certain stage in the rasterise orbit
		std::cout << "from or to have no depth value!" << std::endl;
	}

	float xDiff = to.x - from.x;
	float yDiff = to.y - from.y;
	float zDiff = to.depth - from.depth;
    float numberOfSteps = fmax(std::fmax(abs(xDiff), abs(yDiff)), abs(zDiff));
	if (numberOfSteps == 0){
		return {from};
	}
	float xStepSize = xDiff/numberOfSteps; //how much to increment with each step
	float yStepSize = yDiff/numberOfSteps;
	float zStepSize = zDiff/numberOfSteps;

	std::vector<CanvasPoint> result;
	for (float i=0; i<=numberOfSteps; i++){
		int x = round(from.x + (xStepSize*i));
		int y = round(from.y + (yStepSize*i));
		float z = from.depth + (zStepSize*i);
		
		result.push_back(CanvasPoint(x, y, z));
	}
	return result;
}

std::vector<CanvasPoint> removeDupilcateYs(std::vector<CanvasPoint> line) {
    std::vector<CanvasPoint> result;
	 //takes LHS or RHS

	int lastYValue;
	for (CanvasPoint point: line){
		if (round(point.y)==round(lastYValue)){
			continue;
		} else{
			result.push_back(point);
			lastYValue = round(point.y);
		}
	}
	return result;
}

//to solve this depth issue thing, can consider doing it the other way round, where depth array is initiliased to int max
//might my stupid and not work, just a thought
bool checkPixelDepth (CanvasPoint pixel){
	//reverse camera correction
	//+5 is a VERY jank solution to fix the orbit issue
	pixel.depth = pixel.depth+abs(cameraPosition.z)+10;

	if (!pixel.depth){
		std::cout << "no pixel depth" << std::endl;
		return false;
	}
	if (pixel.depth <= 0.0f){
		//invalid pixel depths corrected by adding 4?
		std::cout << "invalid pixel depth" << pixel.depth << std::endl;
		return false;
	}

	size_t yInt = round (pixel.y);
	size_t xInt = round (pixel.x);

	if (yInt < 0 || yInt >= HEIGHT || xInt < 0 || xInt >= WIDTH) {
		return false;
	}

	if (1/pixel.depth > DepthArray[yInt][xInt] + 0.000001){
		DepthArray[yInt][xInt] = 1/pixel.depth;
		return true;
	}
	else{
		return false;
	}
}

void drawPixelsDepthCheck(std::vector<CanvasPoint> pixels, Colour colour, DrawingWindow &window){
	for (CanvasPoint pixel:pixels){
		if (checkPixelDepth(pixel)){
			window.setPixelColour (round(pixel.x),round(pixel.y), convertColour(colour));			
		} 
	}
}

void filledTriangle (CanvasTriangle triangle, Colour colour, DrawingWindow &window){
	//gets pixels into an array
	std::vector<CanvasPoint> pixels = {triangle.v0(), triangle.v1(), triangle.v2()};
	sortByY(pixels);
	//major side goes from min y to max y. other two sides stop off at pixels[1] on the way
	std::vector<CanvasPoint> sideMinor = lineValuesWithDepth (pixels[0],pixels[1]);
	std::vector<CanvasPoint> sideMinor2 = lineValuesWithDepth (pixels[1],pixels[2]);
    sideMinor.insert(sideMinor.end(), sideMinor2.begin(), sideMinor2.end());
	std::vector<CanvasPoint> sideMajor = lineValuesWithDepth (pixels[0],pixels[2]);

	std::vector<CanvasPoint> LHS;
	std::vector<CanvasPoint> RHS;

	LHS = (pixels[1].x < pixels[2].x) ? sideMinor : sideMajor;
	RHS = (pixels[1].x < pixels[2].x) ? sideMajor : sideMinor;

	std::vector<CanvasPoint> LHSpre = LHS;
	std::vector<CanvasPoint> RHSpre = RHS ;	

	LHS = removeDupilcateYs (LHS);
	RHS = removeDupilcateYs (RHS);

	size_t yDist=pixels[2].y - pixels[0].y;

	for (size_t y=0; y<=yDist; y++){
		//this stops it from trying to draw a line between two point that are the same
		if (!(LHS[y].y == RHS[y].y)){
			continue;
		}

		std::vector<CanvasPoint> line = lineValuesWithDepth(LHS[y], RHS[y]);
		drawPixelsDepthCheck(line, colour, window);
	}
	return;
}

std::vector<CanvasPoint> triangleValues(CanvasTriangle triangle){
	std::vector<CanvasPoint> side1 = lineValuesWithDepth (triangle.v0(),triangle.v1());
	std::vector<CanvasPoint> side2 = lineValuesWithDepth (triangle.v1(),triangle.v2());
	std::vector<CanvasPoint> side3 = lineValuesWithDepth (triangle.v0(),triangle.v2());

	std::vector<CanvasPoint> result;
    result.insert(result.end(), side1.begin(), side1.end());
    result.insert(result.end(), side2.begin(), side2.end());
    result.insert(result.end(), side3.begin(), side3.end());
	return result;
}

float proximityLighting(glm::vec3 point, glm::vec3 lightSource, float s){
	float distance = glm::distance(point,lightSource);
	float intensity = s/(4*pi*(distance*distance));
	if (intensity>1) return 1;
	return intensity;
}

float aoiLighting(glm::vec3 point, glm::vec3 lightSource, glm::vec3 normal, float s){
	glm::vec3 lightDirectionVector = glm::normalize(lightSource-point);
	float intensity = glm::dot(lightDirectionVector,normal);
	if (intensity<0) return 0;
	return intensity;
}

float specularLighting(glm::vec3 point, glm::vec3 lightSource, glm::vec3 normal, glm::vec3 viewVector, float n){
	//same as light direction vector
	glm::vec3 ri = glm::normalize(point-lightSource);
	glm::vec3 rr = glm::normalize(ri - 2.0f * normal * glm::dot(ri, normal));
	float specularExponent = glm::pow((glm::dot(rr,viewVector)),n);

	return specularExponent;
}

std::vector<float> computeBarycentricPoints(glm::vec3 p, ModelTriangle triangle){

	glm::vec3 e0 = triangle.vertices[1] - triangle.vertices[2];
	glm::vec3 e1 = triangle.vertices[0] - triangle.vertices[2];
	glm::vec3 e2 = p - triangle.vertices[2];

	float areaWhole = glm::length(glm::cross(e0,e1));
    float lambda1 = glm::length(glm::cross(e2,e1))/areaWhole;
    float lambda2 = glm::length(glm::cross(e0,e2))/areaWhole;
    float lambda3 = 1.0 - lambda1 - lambda2;

    std::vector<float> result = {lambda1, lambda2, lambda3};
    return result;
}

float gouraud(RayTriangleIntersection intersectionDetails, glm::vec3 lightSource, std::vector<ModelTriangle> &triangles){
		if (findIntersectionBetweenTwoPoints(intersectionDetails.intersectionPoint, lightSource, triangles)){
			return 0.2;
		}

		std::array<float, 3> vertexIntensities;
		for (int i =0; i<3; i++){
			glm::vec3 Vertex = intersectionDetails.intersectedTriangle.vertices[i];
			glm::vec3 VertexNormal = intersectionDetails.intersectedTriangle.normals[i];
			if (findIntersectionBetweenTwoPoints(Vertex, lightSource, triangles)){
				vertexIntensities[i] = 0.2;
				continue;
			}

			glm::vec3 viewVector = glm::normalize(cameraPosition - Vertex);

			float intensityP = proximityLighting(Vertex, lightSource, 5);
			float intensityA = aoiLighting(Vertex, lightSource, VertexNormal, 10);
			float intensityS = specularLighting(Vertex, lightSource, VertexNormal, viewVector, 256);

			vertexIntensities[i] = 0.2+(2*intensityP*intensityA)+intensityS;
			if (vertexIntensities[i]>1) vertexIntensities[i]=1;
		}

		std::vector<float> barycentrics = computeBarycentricPoints(intersectionDetails.intersectionPoint, intersectionDetails.intersectedTriangle);

		//interpolates the intensity of the single point by weighting the barycentrics of the relevant vertices

		//these barycentrics correspond to the correct values at the moment!
		float intensity = barycentrics[1]*vertexIntensities[0] + barycentrics[0]*vertexIntensities[1] + barycentrics[2]*vertexIntensities[2];

		if (intensity>1) return 1;
		return intensity;
}

float phong(RayTriangleIntersection intersectionDetails, std::vector<glm::vec3> lightSources, std::vector<ModelTriangle> &triangles, float extraMultiplier){
	glm::vec3 Vertex = intersectionDetails.intersectionPoint;
	glm::vec3 viewVector = glm::normalize(cameraPosition - Vertex);
	std::array<glm::vec3,3> normals = intersectionDetails.intersectedTriangle.normals;
	std::vector<float> barycentrics = computeBarycentricPoints(intersectionDetails.intersectionPoint, intersectionDetails.intersectedTriangle);
	glm::vec3 vertexNormal = barycentrics[1]*normals[0] + barycentrics[0]*normals[1] + barycentrics[2]*normals[2];

	float ambientIntensity = 0.2;
	float intensityCompensator = lightSources.size();

	float intensity = 0;
	for (glm::vec3 lightSource:lightSources){
		float intensityP = proximityLighting(Vertex, lightSource, 5);
		float intensityA = 2*aoiLighting(Vertex, lightSource, vertexNormal, 10);
		float intensityS = 2*specularLighting(Vertex, lightSource, vertexNormal, viewVector, 256);

		intensityP = glm::clamp(intensityP, 0.0f, 1.0f);
		intensityA = glm::clamp(intensityA, 0.0f, 1.0f);
		intensityS = glm::clamp(intensityS, 0.0f, 1.0f);

		intensity += (intensityP*intensityA)+(intensityS*intensityA) + ambientIntensity;
	}

    float reducedIntensity = extraMultiplier*(intensity / intensityCompensator);
	
	if (reducedIntensity <ambientIntensity) return ambientIntensity;
	if (reducedIntensity>1) return 1;
	return reducedIntensity;
}

//works well, can prob be combined with genericShading now!
float genericShadingMLS(RayTriangleIntersection intersectionDetails, std::vector<glm::vec3> lightSources, glm::vec3 VertexNormal, std::vector<ModelTriangle> &triangles, float extraMultiplier){
	glm::vec3 Vertex = intersectionDetails.intersectionPoint;
	glm::vec3 viewVector = glm::normalize(cameraPosition - Vertex);
	float lightSourceReduction = lightSources.size();
	float ambientIntensity = 0.2;

	float intensity=0;
	for (glm::vec3 lightSource:lightSources){
		if (findIntersectionBetweenTwoPoints(intersectionDetails.intersectionPoint, lightSource, triangles)){
			intensity = intensity + ambientIntensity;
		}
		else{
			float intensityP = 2.5*proximityLighting(Vertex, lightSource, 5);
			float intensityA = aoiLighting(Vertex, lightSource, VertexNormal, 10);
			float intensityS = specularLighting(Vertex, lightSource, VertexNormal, viewVector, 256);
            intensity += (intensityP * intensityA) + intensityS + (ambientIntensity);
		}
	}

    float reducedIntensity = extraMultiplier*(intensity / lightSourceReduction);
	
	if (reducedIntensity <ambientIntensity) return ambientIntensity;
	if (reducedIntensity>1) return 1;
	return reducedIntensity;
}

float genericShading(RayTriangleIntersection intersectionDetails, std::vector<glm::vec3> lightSources, std::vector<ModelTriangle> &triangles, glm::vec3 VertexNormal){
	//redirects to generic shading for multiple light sources
	if (lightSources.size()>1){
		return genericShadingMLS(intersectionDetails, lightSources, VertexNormal, triangles, 1); //final value is the lightsource multiplier. let this be controlled by main for the final sequence
	}
	glm::vec3 lightSource = lightSources[0];
	glm::vec3 Vertex = intersectionDetails.intersectionPoint;
	glm::vec3 viewVector = glm::normalize(cameraPosition - Vertex);

	float intensity;
	if (findIntersectionBetweenTwoPoints(intersectionDetails.intersectionPoint, lightSource, triangles)){
		intensity = 0.2;
	}
	else{
		float intensityP = 2.5*proximityLighting(Vertex, lightSource, 5);
		float intensityA = aoiLighting(Vertex, lightSource, VertexNormal, 10);
		float intensityS = specularLighting(Vertex, lightSource, VertexNormal, viewVector, 256);
		intensity = 0.2+(intensityP*intensityA)+intensityS;
	}


	if (intensity>1) return 1;
	return intensity;
}

glm::vec3 randomlyChange(glm::vec3 normal, float strength){
    std::random_device rd;
    std::mt19937 gen(rd());
	std::normal_distribution<> Ndist(0.0, 0.005);
    for (int i=0; i<3; i++){
		float rand = Ndist(gen);
		normal[i] = normal[i]+(rand*strength);
	}
	return normal;
}

Colour texture3D(RayTriangleIntersection intersectionDetails, TextureMap &texture){
	// if (USE_TEXTURED_FLOOR==0){
	// 	return intersectionDetails.intersectedTriangle.colour;
	// }
	std::vector<float> barycentrics = computeBarycentricPoints(intersectionDetails.intersectionPoint,intersectionDetails.intersectedTriangle);

	float u = barycentrics[1]*intersectionDetails.intersectedTriangle.texturePoints[0].x+
				barycentrics[0]*intersectionDetails.intersectedTriangle.texturePoints[1].x+
				barycentrics[2]*intersectionDetails.intersectedTriangle.texturePoints[2].x;

	float v = barycentrics[1]*intersectionDetails.intersectedTriangle.texturePoints[0].y+
				barycentrics[0]*intersectionDetails.intersectedTriangle.texturePoints[1].y+
				barycentrics[2]*intersectionDetails.intersectedTriangle.texturePoints[2].y;

    int textureXdistance = static_cast<int>(u * texture.width);
    int textureYdistance = static_cast<int>(v * texture.height);


	//grab pixel
	uint32_t pixel = texture.pixels[textureYdistance * texture.width + textureXdistance];	//convert to RGB format so it can be manipulated by intensity later
	uint8_t r = (pixel >> 16) & 0xFF;
	uint8_t g = (pixel >> 8) & 0xFF;
	uint8_t b = pixel & 0xFF;

	return Colour(r,g,b);
}

class Skybox {
public:
    Skybox(TextureMap& backTexture, TextureMap& bottomTexture,
           TextureMap& frontTexture, TextureMap& leftTexture, TextureMap& rightTexture, 
           TextureMap& topTexture)
        : backTexture(backTexture), bottomTexture(bottomTexture),
          frontTexture(frontTexture), leftTexture(leftTexture), rightTexture(rightTexture), 
          topTexture(topTexture) {}

    uint32_t getSkyboxPixel(glm::vec3 rayDirection) {
        int u = 0;
        int v = 0;

		if (USE_SKYBOX==0){
			return 0;
		}

		rayDirection=glm::normalize(rayDirection);

		if (std::isnan(rayDirection[0]) || std::isnan(rayDirection[1]) || std::isnan(rayDirection[2])){
			std::cout<<"skybox nan!, will early return: "<<rayDirection[0]<<","<<rayDirection[1]<<","<<rayDirection[2]<<std::endl;
			return convertColour(Colour(255,0,255));
		}

        float dominantDirection = std::fmax(std::fmax(abs(rayDirection[0]), abs(rayDirection[1])), abs(rayDirection[2]));


        int pixels_width = backTexture.width;
        int pixels_height = backTexture.height;

        if (dominantDirection == abs(rayDirection[0])) { //left or right
            if (rayDirection[0] > 0) {
                u = round((-rayDirection[2] / abs(rayDirection[0]) + 1.0f) * 0.5f * (pixels_width - 1));
                v = round((-rayDirection[1] / abs(rayDirection[0]) + 1.0f) * 0.5f * (pixels_height - 1));
                return rightTexture.pixels[v * pixels_width + u];
            } else {
                u = round((rayDirection[2] / abs(rayDirection[0]) + 1.0f) * 0.5f * (pixels_width - 1));
                v = round((-rayDirection[1] / abs(rayDirection[0]) + 1.0f) * 0.5f * (pixels_height - 1));
                return leftTexture.pixels[v * pixels_width + u];
            }
        } else if (dominantDirection == abs(rayDirection[1])) { //top or bottom
            if (rayDirection[1] > 0) { //top
                u = round((rayDirection[0] / abs(rayDirection[1]) + 1.0f) * 0.5f * (pixels_width - 1));
                v = round((rayDirection[2] / abs(rayDirection[1]) + 1.0f) * 0.5f * (pixels_height - 1));
                return topTexture.pixels[v * pixels_width + u];
            } else { //bottom
                u = round((rayDirection[0] / abs(rayDirection[1]) + 1.0f) * 0.5f * (pixels_width - 1));
                v = round((-rayDirection[2] / abs(rayDirection[1]) + 1.0f) * 0.5f * (pixels_height - 1));
                return bottomTexture.pixels[v * pixels_width + u];
            }
        } else if (dominantDirection == abs(rayDirection[2])) { //front or back
            if (rayDirection[2] > 0) {
                u = round((rayDirection[0] / abs(rayDirection[2]) + 1.0f) * 0.5f * (pixels_width - 1));
                v = round((-rayDirection[1] / abs(rayDirection[2]) + 1.0f) * 0.5f * (pixels_height - 1));
                return frontTexture.pixels[v * pixels_width + u];
            } else {
                u = round((-rayDirection[0] / abs(rayDirection[2]) + 1.0f) * 0.5f * (pixels_width - 1));
                v = round((-rayDirection[1] / abs(rayDirection[2]) + 1.0f) * 0.5f * (pixels_height - 1));
                return backTexture.pixels[v * pixels_width + u];
            }
        } else {
            std::cout << "fatal skybox error!" << std::endl;
            std::cout << rayDirection[0] << std::endl;
            std::cout << rayDirection[1] << std::endl;
            std::cout << rayDirection[2] << std::endl;
            std::cout << dominantDirection << std::endl;
            return 0;
        }
    }

private:
    TextureMap backTexture;
    TextureMap bottomTexture;
    TextureMap frontTexture;
    TextureMap leftTexture;
    TextureMap rightTexture;
    TextureMap topTexture;
};

glm::vec3 convertToNormalVector(Colour colour) {
    //scale r and g to [-1,1], scale b to [0,1]
    float r = ((colour.red / 255.0)*2)-1;
    float g = ((colour.green / 255.0)*2)-1;
    float b = colour.blue / 255.0;

	//assemble and return
    glm::vec3 normal(r, g, b);
    return glm::normalize(normal);
}

Colour uint32ToColour(uint32_t color) {
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    
    return Colour(r, g, b);
}

Colour combineColours(float weight, const Colour &colour1, const Colour &colour2) {
    int red = static_cast<int>(weight * colour1.red + (1.0f - weight) * colour2.red);
    int green = static_cast<int>(weight * colour1.green + (1.0f - weight) * colour2.green);
    int blue = static_cast<int>(weight * colour1.blue + (1.0f - weight) * colour2.blue);
    return Colour(red, green, blue);
}

std::pair<float, Colour> shootRay(std::vector<ModelTriangle> &triangles, std::unordered_map<int, std::string> &indexToFile, glm::vec3 rayDirection, std::vector<glm::vec3> &lightSources, RayTriangleIntersection intersectionDetails, TextureMap &texture,
TextureMap &normalMap1, TextureMap &normalMap2, TextureMap &normalMap3, TextureMap &normalMap4, int redirectCount, Skybox &skybox){

	Colour oldColour = Colour (255,255,255);
	float intensity;
	int maxRedirects = 15;
	redirectCount=redirectCount+1;

	if (redirectCount>maxRedirects){ 
		return {1, Colour (255,255,255)};
		// return {1, intersectionDetails.intersectedTriangle.colour};
	}

	if (intersectionDetails.distanceFromCamera==-1){
		uint32_t skyboxColour = skybox.getSkyboxPixel(rayDirection);
		return {1, uint32ToColour(skyboxColour)};
	}

	if (indexToFile[intersectionDetails.triangleIndex]=="cornell-box"){
		intensity = genericShading(intersectionDetails, lightSources, triangles, intersectionDetails.intersectedTriangle.normal);
		oldColour = intersectionDetails.intersectedTriangle.colour;
		redirectCount=maxRedirects;
		
	} else if (indexToFile[intersectionDetails.triangleIndex]=="sphere"){
		intensity = phong(intersectionDetails, lightSources, triangles, 1);
		oldColour = intersectionDetails.intersectedTriangle.colour;
		redirectCount=maxRedirects;

	} else if (indexToFile[intersectionDetails.triangleIndex]=="textured-floor"){
		intensity = genericShading(intersectionDetails, lightSources, triangles, intersectionDetails.intersectedTriangle.normal);
		oldColour = texture3D(intersectionDetails, texture);
		redirectCount=maxRedirects;

	} else if (indexToFile[intersectionDetails.triangleIndex] == "glass") {
		glm::vec3 normal = intersectionDetails.intersectedTriangle.normal;

		//refractive indices, ri1 = air by default, ri2 = glass by default
		float ri1 = 1.0f;
		float ri2 = 1.4f;
		
		//work out which side of the material the ray is
		float cosI = glm::dot(-rayDirection, normal);
		bool entering = cosI > 0;
		
		//swap refractive indices and relevant vectors
		if (!entering) {
			normal = -normal;
			cosI = -cosI;
			std::swap(ri1, ri2);
		}
		
		float riRatio = ri1 / ri2;
		float k = 1.0f - riRatio * riRatio * (1.0f - cosI * cosI);
		
		glm::vec3 newDirection;
		if (k < 0.0f) {
			//total internal reflection
			newDirection = rayDirection - 2.0f * glm::dot(rayDirection, normal) * normal;
		} else {
			//apply refraction with snella law
			newDirection = riRatio * rayDirection + (riRatio * cosI - sqrt(k)) * normal;
			newDirection = glm::normalize(newDirection);
		}
		
		glm::vec3 intPoint = intersectionDetails.intersectionPoint + newDirection * 0.001f;
		
		intersectionDetails = getClosestIntersection(newDirection, triangles, intPoint);
		auto refrPair = shootRay(triangles, indexToFile, newDirection, lightSources, 
								intersectionDetails, texture, normalMap1, normalMap2, 
								normalMap3, normalMap4, redirectCount, skybox);
		
		return {refrPair.first, refrPair.second};
		//could add reflection with reflection coefficient

	//did this long form to save time
	} else if (indexToFile[intersectionDetails.triangleIndex]=="normal-map1"){
		Colour vertexNormalAsColour = texture3D(intersectionDetails, normalMap1);
		glm::vec3 vertexNormal = convertToNormalVector(vertexNormalAsColour);
		vertexNormal = glm::normalize(vertexNormal);
		intensity = genericShading(intersectionDetails, lightSources, triangles, vertexNormal);
		oldColour = intersectionDetails.intersectedTriangle.colour;
		redirectCount=maxRedirects;
	} else if (indexToFile[intersectionDetails.triangleIndex]=="normal-map2"){
		Colour vertexNormalAsColour = texture3D(intersectionDetails, normalMap2);
		glm::vec3 vertexNormal = convertToNormalVector(vertexNormalAsColour);
		vertexNormal = glm::normalize(vertexNormal);
		intensity = genericShading(intersectionDetails, lightSources, triangles, -vertexNormal);
		oldColour = intersectionDetails.intersectedTriangle.colour;
		redirectCount=maxRedirects;
	} else if (indexToFile[intersectionDetails.triangleIndex]=="normal-map3"){
		Colour vertexNormalAsColour = texture3D(intersectionDetails, normalMap3);
		glm::vec3 vertexNormal = convertToNormalVector(vertexNormalAsColour);
		vertexNormal = glm::normalize(vertexNormal);
		intensity = genericShading(intersectionDetails, lightSources, triangles, -vertexNormal);
		oldColour = intersectionDetails.intersectedTriangle.colour;
		redirectCount=maxRedirects;
	} else if (indexToFile[intersectionDetails.triangleIndex]=="normal-map4"){
		Colour vertexNormalAsColour = texture3D(intersectionDetails, normalMap4);
		glm::vec3 vertexNormal = convertToNormalVector(vertexNormalAsColour);
		vertexNormal = glm::normalize(vertexNormal);
		intensity = genericShading(intersectionDetails, lightSources, triangles, -vertexNormal);
		oldColour = intersectionDetails.intersectedTriangle.colour;
		redirectCount=maxRedirects;

	} else if (indexToFile[intersectionDetails.triangleIndex]=="mirror"){ //recursive call
		glm::vec3 normal =intersectionDetails.intersectedTriangle.normal;
		if (METALIC_MIRROR == 1){
			normal = randomlyChange(intersectionDetails.intersectedTriangle.normal,1);
		}
		rayDirection = glm::normalize(rayDirection - 2.0f * glm::dot(rayDirection, normal) * normal);

		if (std::isnan(rayDirection[0]) || std::isnan(rayDirection[1]) || std::isnan(rayDirection[2])){
			std::cout<<"nan mirror reflection: "<<rayDirection[0]<<","<<rayDirection[1]<<","<<rayDirection[2]<<std::endl; //got a -nan!
		}

		glm::vec3 intPoint = glm::vec3(intersectionDetails.intersectionPoint[0]+0.01,intersectionDetails.intersectionPoint[1]+0.01,intersectionDetails.intersectionPoint[2]+0.01);
		intersectionDetails = getClosestIntersection(rayDirection, triangles, intPoint);
		auto bounced = shootRay(triangles, indexToFile, rayDirection, lightSources, intersectionDetails, texture, normalMap1, normalMap2, normalMap3, normalMap4, redirectCount,skybox);

		intensity = bounced.first;
		oldColour = bounced.second;


	}else{
		oldColour = Colour (255,255,255);
		intensity = 0;
		std::cout<<"sticky one still, check for indexToFile error"<<std::endl;
	}
	if (std::isnan(rayDirection[0])||std::isnan(rayDirection[1])||std::isnan(rayDirection[2])){
		std::cout<<"nan ray direction3: "<<rayDirection[0]<<","<<rayDirection[1]<<","<<rayDirection[2]<<std::endl; //got a -nan!
	}
	return {intensity, oldColour};
}

void drawRayTraced (int startY, int endY, std::vector<ModelTriangle> &triangles, DrawingWindow &window, std::unordered_map<int, std::string> &indexToFile, std::vector<glm::vec3> &lightSources) {    
	TextureMap backTexture = GDB_FILES ? TextureMap("../assets/skybox/back.ppm") : TextureMap("./assets/skybox/back.ppm");
	TextureMap bottomTexture = GDB_FILES ? TextureMap("../assets/skybox/bottom.ppm") : TextureMap("./assets/skybox/bottom.ppm");
	TextureMap frontTexture = GDB_FILES ? TextureMap("../assets/skybox/front.ppm") : TextureMap("./assets/skybox/front.ppm");
	TextureMap leftTexture = GDB_FILES ? TextureMap("../assets/skybox/left.ppm") : TextureMap("./assets/skybox/left.ppm");
	TextureMap rightTexture = GDB_FILES ? TextureMap("../assets/skybox/right.ppm") : TextureMap("./assets/skybox/right.ppm");
	TextureMap topTexture = GDB_FILES ? TextureMap("../assets/skybox/top.ppm") : TextureMap("./assets/skybox/top.ppm");

	//initilise skybox object
	Skybox skybox(backTexture, bottomTexture, frontTexture, leftTexture, rightTexture, topTexture);

	// std::cout<<"skybox loaded"<<std::endl;

	TextureMap texture = GDB_FILES ? TextureMap("../assets/texture2.ppm") : TextureMap("./assets/texture2.ppm");
	TextureMap normalMap1 = GDB_FILES ? TextureMap("../assets/normalMap1.ppm") : TextureMap("./assets/normalMap1.ppm");
	TextureMap normalMap2 = GDB_FILES ? TextureMap("../assets/normalMap2.ppm") : TextureMap("./assets/normalMap2.ppm");
	TextureMap normalMap3 = GDB_FILES ? TextureMap("../assets/normalMap3.ppm") : TextureMap("./assets/normalMap3.ppm");
	TextureMap normalMap4 = GDB_FILES ? TextureMap("../assets/normalMap4.ppm") : TextureMap("./assets/normalMap4.ppm");


	int focalLength = 2;
    for (size_t y = startY; y < endY; y++) {
        for (size_t x = 0; x < WIDTH; x++) {
            float u = (x - WIDTH / 2.0f) / (WIDTH / focalLength);
            float v = -(y - HEIGHT / 2.0f) / (WIDTH / focalLength);

            glm::vec3 pixelInCam = glm::vec3(u, v, -focalLength);
            glm::vec3 rayDirection = glm::normalize(pixelInCam * cameraOrientation);

            RayTriangleIntersection intersectionDetails = getClosestIntersection(rayDirection, triangles, cameraPosition);
			
			//dont do shading on the outside of the box
			glm::vec3 pointToCam = glm::normalize(intersectionDetails.intersectionPoint - cameraPosition);
			if (glm::dot(intersectionDetails.intersectedTriangle.normal, pointToCam) > 0) {	
				Colour oldColour = intersectionDetails.intersectedTriangle.colour;
				Colour newColour (oldColour.red*0.4, oldColour.green*0.4, oldColour.blue*0.4);
				window.setPixelColour(x, y, convertColour(newColour));
				continue;
			}

			if (intersectionDetails.distanceFromCamera != -1) {
				auto result = shootRay(triangles, indexToFile, rayDirection, lightSources, intersectionDetails, texture, normalMap1, normalMap2, normalMap3, normalMap4, 0, skybox);

				float intensity = result.first;
				Colour oldColour = result.second;

				Colour newColour;
				newColour = Colour(oldColour.red*intensity, oldColour.green*intensity, oldColour.blue*intensity);

				window.setPixelColour(x, y, convertColour(newColour));

			} else{ //skybox time
				uint32_t skyboxColour = skybox.getSkyboxPixel(rayDirection);
				window.setPixelColour(x, y, skyboxColour);
			}
        }
    }	
}

//intermediate chatGPT function to make multiple cores/threads work on raytracing
//ensure that in makefile:
//COMPILER_OPTIONS := -c -pthread -pipe -Wall -std=c++11 # If you have an older compiler, you might have to use -std=c++0x
//LINKER_OPTIONS := -pthread
void drawRayTracedParallelise(std::vector<ModelTriangle> &triangles, DrawingWindow &window, std::unordered_map<int, std::string> indexToFile, std::vector<glm::vec3> lightSources) {
    const int numThreads = 8;  // Number of threads you want to use
    int sectionHeight = HEIGHT / numThreads;

    std::vector<std::thread> threads;

    // Launch threads to render different sections of the screen
    for (int i = 0; i < numThreads; i++) {
        int startY = i * sectionHeight;
        int endY = (i == numThreads - 1) ? HEIGHT : (i + 1) * sectionHeight;
        threads.push_back(std::thread(drawRayTraced, startY, endY, std::ref(triangles), std::ref(window), std::ref(indexToFile), std::ref(lightSources)));
    }

    // Join all threads
    for (auto& t : threads) {
        t.join();
    }
}

void draw3DTriangles(std::vector<ModelTriangle> &triangles,  DrawingWindow &window){
	for (ModelTriangle triangle:triangles){
		float focalLength = 2;
		CanvasPoint v0 = projectVertexOntoCanvasPoint(cameraPosition, focalLength, triangle.vertices[0]);
		CanvasPoint v1 = projectVertexOntoCanvasPoint(cameraPosition, focalLength, triangle.vertices[1]);
		CanvasPoint v2 = projectVertexOntoCanvasPoint(cameraPosition, focalLength, triangle.vertices[2]);

		CanvasTriangle triangle2D(v0, v1, v2);

		//for filled triangles with the correct colour
		filledTriangle(triangle2D, triangle.colour, window);
		std::vector<CanvasPoint> triangleValues1 = triangleValues(triangle2D);
		drawPixelsDepthCheck(triangleValues1, triangle.colour, window);
	}
}

void drawWireframe(std::vector<ModelTriangle> &triangles,  DrawingWindow &window){
	for (ModelTriangle triangle:triangles){
		float focalLength = 2;
		CanvasPoint v0 = projectVertexOntoCanvasPoint(cameraPosition, focalLength, triangle.vertices[0]);
		CanvasPoint v1 = projectVertexOntoCanvasPoint(cameraPosition, focalLength, triangle.vertices[1]);
		CanvasPoint v2 = projectVertexOntoCanvasPoint(cameraPosition, focalLength, triangle.vertices[2]);

		CanvasTriangle triangle2D(v0, v1, v2);

		std::vector<CanvasPoint> triangleValues1 = triangleValues(triangle2D);
		drawPixelsDepthCheck(triangleValues1, triangle.colour, window);
	}
}

//pan functions multiply cameraOrientation by rotationMatrix, for radians t
void pan(float t){
	glm::mat3 XrotationMatrix ({cos(t),0,-sin(t)},{0,1,0},{sin(t),0,cos(t)});
	cameraOrientation=cameraOrientation*XrotationMatrix;
}

void tilt(float t){
	glm::mat3 YrotationMatrix ({1,0,0},{0,cos(t),sin(t)},{0,-sin(t),cos(t)});
	cameraOrientation=cameraOrientation*YrotationMatrix;
}

void rotateAboutOrigin (float d){
	glm::mat3 rotationMatrix ({cos(d),0,-sin(d)},{0,1,0},{sin(d),0,cos(d)});
	cameraPosition=rotationMatrix*cameraPosition;
}

void lookAt (glm::vec3 p){
	//get vector from camera to point
	glm::vec3 forward = glm::normalize(cameraPosition-p);

	//corrects for when the camera goes behind the origin
	// if (glm::dot(forward, p-cameraPosition)>0){
	// 	forward=-forward;
	// 	std::cout << "swapped" <<std::endl;
	// }
	glm::vec3 vertical (0,1,0);
	glm::vec3 right = glm::normalize(glm::cross(vertical,forward));
	glm::vec3 up = glm::normalize(glm::cross(forward,right));
	cameraOrientation = glm::transpose(glm::mat3(right,up,forward));
}

float convertDegrees(float degrees) {
    return degrees * (pi / 180);
}

int render(std::vector<ModelTriangle> &triangles,  DrawingWindow &window, std::unordered_map<int, std::string> indexToFile, std::vector<glm::vec3> lightSources, int frameNumber){
	if (renderMode==WIREFRAME){
		drawWireframe(triangles,window);
	} else if (renderMode==RASTERISE){
		draw3DTriangles(triangles, window);
	} else if (renderMode==RAYTRACE){
		drawRayTracedParallelise(triangles, window, indexToFile, lightSources);
	} else{
		std::cout<<"render type unknown"<<std::endl;
	}
	// window.savePPM("frame_" + std::to_string(frameNumber) + ".ppm");

	window.renderFrame();

	return frameNumber++;
	// std::cout<<"render completed"<<std::endl;
}

std::vector<glm::vec3> softShadowsLightSources (glm::vec3 lightSource, float radius, int n){
    std::vector<glm::vec3> lightSources;

    for (int i = 0; i < n; i++) {
		std::mt19937 generator(std::random_device{}());
		std::normal_distribution<float> distribution(0, radius);
		float xPos = distribution(generator);
		float zPos = distribution(generator);
		float yPos = distribution(generator);

		//gets rid of the possibility of extreme values
		if (abs(xPos) > radius || abs(yPos) > radius || abs(zPos) > radius){
			n++;
			continue;
		}

        lightSources.push_back(lightSource + glm::vec3(xPos, yPos, zPos));
    }

    return lightSources;
}

void moveSmoothly(glm::vec3 from, glm::vec3 to, glm::mat3 startingOrientation, glm::mat3 endingOrientation, int numberSteps,
std::vector<ModelTriangle> &triangles,  DrawingWindow &window, std::unordered_map<int, std::string> indexToFile, std::vector<glm::vec3> lightSources, int frameNumber) {

	cameraPosition = from;
	cameraOrientation = startingOrientation;

    for (int step = 0; step <= numberSteps; ++step) {
		window.clearPixels();
		memset(DepthArray, 0, sizeof(DepthArray));

		float xDiff = to.x - from.x;
		float yDiff = to.y - from.y;
		float zDiff = to.z - from.z;

		float xStepSize = xDiff/numberSteps;
		float yStepSize = yDiff/numberSteps;
		float zStepSize = zDiff/numberSteps;

        float t = float(step) / float(numberSteps);
        
        glm::mat3 currentOrientation = glm::mat3(
            glm::mix(startingOrientation[0], endingOrientation[0], t),
            glm::mix(startingOrientation[1], endingOrientation[1], t),
            glm::mix(startingOrientation[2], endingOrientation[2], t)
        );

        // Update the camera's orientation
        cameraOrientation = currentOrientation;

		cameraPosition= glm::vec3(cameraPosition[0]+ xStepSize, cameraPosition[1]+ yStepSize, cameraPosition[2]+ zStepSize);

		frameNumber = render(triangles, window, indexToFile, lightSources, 1);
    }
}

void printVec3(const glm::vec3& vec) {
    std::cout << "Vec3: (" 
              << vec.x << ", " 
              << vec.y << ", " 
              << vec.z << ")" << std::endl;
}

void printMat3(const glm::mat3& mat) {
    std::cout << "Mat4:" << std::endl;
    for (int i = 0; i < 3; ++i) {
        std::cout << "(";
        for (int j = 0; j < 3; ++j) {
            std::cout << mat[i][j] << " ";
        }
        std::cout << ")" << std::endl;
    }
}

int main(int argc, char *argv[]) {
	DrawingWindow window = DrawingWindow(WIDTH, HEIGHT, false);
	SDL_Event event;

	//given a triangle index, will reveal which file it came from
	std::unordered_map<int, std::string> indexToFile;

	std::tuple<std::vector<Colour>, std::vector<std::string>, std::vector<std::string>> colours = MTLparser ("./assets/textured-cornell-box.mtl");

	//returns texture files to use (3rd item in the tuple) not actually being used, am hard coding


	//have hardcoded texture file to remove instances of cobbles (replaced with green)
	std::vector<ModelTriangle> trianglesCornelBox = OBJparser ("./assets/textured-cornell-box.obj", colours, 0.35, glm::vec3(0,0,0));
	for (int i=0; i<trianglesCornelBox.size(); i++){
		indexToFile[i] = "cornell-box";
	}
	std::vector<ModelTriangle> triangles = trianglesCornelBox;
	std::vector<ModelTriangle> trianglesSphere = OBJparser ("./assets/sphere.obj", colours, 0.35, glm::vec3(0.6,0.1,-0.8));
	for (int i=triangles.size(); i<triangles.size()+trianglesSphere.size(); i++){
		indexToFile[i] = "sphere";
	}
	triangles.insert(triangles.end(), trianglesSphere.begin(), trianglesSphere.end());

	std::vector<ModelTriangle> trianglesBunny = OBJparser ("./assets/cornell-bunny.obj", colours, 0.35, glm::vec3(0,0,0));
	for (int i=triangles.size(); i<triangles.size()+trianglesBunny.size(); i++){
		indexToFile[i] = "glass";
		// indexToFile[i] = "cornell-box";

	}
	triangles.insert(triangles.end(), trianglesBunny.begin(), trianglesBunny.end());


	std::cout<<triangles.size()<<std::endl;


	//blue box is indexes 22-31

	//hardcodes front of blue box to be a mirror
	if (USE_MIRROR){
		indexToFile[26] = "mirror";
		indexToFile[31] = "mirror";
	}

	//hardcodes the floor to be textured
	indexToFile[6] = "textured-floor";
	indexToFile[7] = "textured-floor";

	//hardcodes the top of the red box to be normal map
	// indexToFile[12] = "normal-map";
	// indexToFile[17] = "normal-map";

	//hardcodes left wall to be normal map
	indexToFile[8] = "normal-map1";
	indexToFile[9] = "normal-map1";

	//first red box index = 12
	//hardcodes each side of the red box to be a different normal mao
	// indexToFile [12] = "mirror"; // top left
	// indexToFile [17] = "mirror"; //top right

	//top 12,17
	//front 14,19
	//right 15,20
	

	indexToFile [13] = "normal-map1"; //back
	indexToFile [18] = "normal-map1"; //back

	indexToFile [14] = "normal-map1"; //front
	indexToFile [19] = "normal-map1"; //front

	indexToFile [15] = "normal-map1"; //right
	indexToFile [20] = "normal-map1"; //right

	indexToFile [16] = "normal-map1"; //right
	indexToFile [21] = "normal-map1"; //right

	indexToFile [17] = "normal-map1"; //right
	indexToFile [22] = "normal-map1"; //right


    for (int i = 12; i <= 21; ++i) {
        indexToFile[i] = "normal-map1";
    }



	glm::vec3 startingCamera (0.0, 0.0, 4.0);
	cameraPosition = startingCamera;	
	bool orbit = 0;

	int frameNumber = 0;


	//far corners
	//bl, br, fl, fr
	

	// std::vector<glm::vec3> lightSources = softShadowsLightSources (glm::vec3(0,0.8,0), 0.03, 4);
	std::vector<glm::vec3> lightSources = {glm::vec3(0,0.25,0.9), glm::vec3(0.9, 0.25, 0.9)};
	// std::vector<glm::vec3> lightSources = {glm::vec3(0,0.8,0)};


	//flight corners

	//middle
	// std::vector<glm::vec3> lightSources {
	// 	glm::vec3 (0,0.9,0)
	// };



	while (true) {

		window.clearPixels();
		memset(DepthArray, 0, sizeof(DepthArray));
		if (orbit==1){
			rotateAboutOrigin(convertDegrees(1));
			lookAt(glm::vec3 (0,0,0));	
			frameNumber = render(triangles, window, indexToFile, lightSources, 1);

		}
		// We MUST poll for events - otherwise the window will freeze !
		if (window.pollForInputEvents(event)){
			//polls for key presses
			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_o){
				std::cout<<"orbit toggle"<<std::endl;
				orbit=abs(orbit-1);
			}
			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_w){
				std::cout << "move forward" << std::endl;
				cameraPosition.z=cameraPosition.z-0.1;
			}
			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_s){
				std::cout << "move backward" << std::endl;
				cameraPosition.z=cameraPosition.z+0.1;
			}
			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_a){
				std::cout << "move left" << std::endl;
				cameraPosition.x=cameraPosition.x-0.1;
			}
			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_d){
				std::cout << "move right" << std::endl;
				cameraPosition.x=cameraPosition.x+0.1;
			}
			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_p){
				std::cout << "move up" << std::endl;
				cameraPosition.y=cameraPosition.y+0.1;
			}
			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_l){
				std::cout << "move up" << std::endl;
				cameraPosition.y=cameraPosition.y-0.1;
			}
			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_DOWN){
				std::cout << "look down" << std::endl;
				tilt(convertDegrees(1.5));
			}
			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_UP){
				std::cout << "look up" << std::endl;
				tilt(convertDegrees(-1.5));
			}
			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_RIGHT){
				std::cout << "look right" << std::endl;
				pan(convertDegrees(1.5));
			}
			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_LEFT){
				std::cout << "look left" << std::endl;
				pan(convertDegrees(-1.5));
			}
			// if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_p){
			// 	std::cout << "look at origin" << std::endl;
			// 	lookAt(glm::vec3 (0,0,0));
			// }
			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_b){
				std::cout << "b down - wireframe" << std::endl;
				renderMode = WIREFRAME;
			}
			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_n){
				std::cout << "n down - rasterise" << std::endl;
				renderMode = RASTERISE;
				render(triangles, window, indexToFile, lightSources,0);
				window.renderFrame();
			}
			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_m){
				std::cout << "m down - raytrace" << std::endl;
				renderMode = RAYTRACE;
				render(triangles, window, indexToFile, lightSources,0);
				window.renderFrame();
			}
			// if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_j){
			// 	std::cout << "j down - proximity lighting toggle" << std::endl;
			// 	std::cout <<lightingMode[0]<<lightingMode[1]<<lightingMode[2]<<std::endl;
			// 	lightingMode[0] = abs(lightingMode[0]-1);
			// }
			// if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_k){
			// 	std::cout << "k down - area of incidence lighting toggle" << std::endl;
			// 	std::cout <<lightingMode[0]<<lightingMode[1]<<lightingMode[2]<<std::endl;
			// 	lightingMode[1] = abs(lightingMode[1]-1);
			// }
			// if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_r){
			// 	render(triangles, window, indexToFile, lightSources,0);
			// 	window.renderFrame();
			// }
			// if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_l){
			// 	std::cout<<"l down - switch lightsource"<<std::endl;
			// 	if (lightSources.size()>1){
			// 			lightSources =  {
			// 				glm::vec3(0, 0.9f, 0)};
			// 	} else{
			// 			lightSources =  {
			// 				glm::vec3(0, 0.9f, 0),
			// 				glm::vec3(0, 0.9f, 0),
			// 				glm::vec3(0, 0.9f, 0),
			// 				glm::vec3(0, 0.9f, 0)
			// 			};
			// 	}
			// 	render(triangles, window, indexToFile, lightSources,0);
			// 	window.renderFrame();
			// 	std::cout<<"light source switched"<<std::endl;

			// }
			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_h){
				std::cout<<"h down moving smoothly"<<std::endl;
				moveSmoothly(glm::vec3(0.0, 0.0, 4.0), glm::vec3(0.0, 0.0, 0), glm::mat3 ({1,0,0},{0,1,0},{0,0,1}), glm::mat3 ({0, 0, -1}, {0, 1, 0}, {1, 0, 0}), 20,
				triangles, window, indexToFile, lightSources, 1); //these added so that rendering can be done within the functiuon
			};
			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_c){
				std::cout<<"c down print location and orientation"<<std::endl;
				printVec3(cameraPosition);
				printMat3(cameraOrientation);

			};
			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_1){
				std::cout<<"1 down - spin red box"<<std::endl;
			
				float angleToChange = convertDegrees(6);
				for (int s=0; s<60; s++) {
					window.clearPixels();
					memset(DepthArray, 0, sizeof(DepthArray));

					float topMiddleX1 = (triangles[12].vertices[0].x + triangles[12].vertices[1].x + triangles[12].vertices[2].x) / 3;
					float topMiddleY1 = (triangles[12].vertices[0].y + triangles[12].vertices[1].y + triangles[12].vertices[2].y) / 3;
					float topMiddleZ1 = (triangles[12].vertices[0].z + triangles[12].vertices[1].z + triangles[12].vertices[2].z) / 3;

					float topMiddleX2 = (triangles[17].vertices[0].x + triangles[17].vertices[1].x + triangles[17].vertices[2].x) / 3;
					float topMiddleY2 = (triangles[17].vertices[0].y + triangles[17].vertices[1].y + triangles[17].vertices[2].y) / 3;
					float topMiddleZ2 = (triangles[17].vertices[0].z + triangles[17].vertices[1].z + triangles[17].vertices[2].z) / 3;

					float botMiddleX1 = (triangles[14].vertices[0].x + triangles[14].vertices[1].x + triangles[14].vertices[2].x) / 3;
					float botMiddleY1 = (triangles[14].vertices[0].y + triangles[14].vertices[1].y + triangles[14].vertices[2].y) / 3;
					float botMiddleZ1 = (triangles[14].vertices[0].z + triangles[14].vertices[1].z + triangles[14].vertices[2].z) / 3;

					float botMiddleX2 = (triangles[16].vertices[0].x + triangles[16].vertices[1].x + triangles[16].vertices[2].x) / 3;
					float botMiddleY2 = (triangles[16].vertices[0].y + triangles[16].vertices[1].y + triangles[16].vertices[2].y) / 3;
					float botMiddleZ2 = (triangles[16].vertices[0].z + triangles[16].vertices[1].z + triangles[16].vertices[2].z) / 3;

					float centroidX = (topMiddleX1 + topMiddleX2 + botMiddleX1 + botMiddleX2) / 4;
					float centroidY = (topMiddleY1 + topMiddleY2 + botMiddleY1 + botMiddleY2) / 4;
					float centroidZ = (topMiddleZ1 + topMiddleZ2 + botMiddleZ1 + botMiddleZ2) / 4;

					for (int j = 12; j < 22; j++) {
						for (int k = 0; k < 3; k++) {
							float x = triangles[j].vertices[k].x - centroidX;
							float y = triangles[j].vertices[k].y - centroidY;
							float z = triangles[j].vertices[k].z - centroidZ;

							float rotatedX = x * cos(angleToChange) + z * sin(angleToChange);
							float rotatedZ = -x * sin(angleToChange) + z * cos(angleToChange);

							triangles[j].vertices[k].x = rotatedX + centroidX;
							triangles[j].vertices[k].y = y + centroidY;
							triangles[j].vertices[k].z = rotatedZ + centroidZ;
						}
					}

					
					frameNumber = render(triangles, window, indexToFile, lightSources, 1);
				}

			};
			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_f){
				std::cout << "f down, move up and then do a backflip" << std::endl;

				//move rectangle up
				for (int i = 0; i < 2; i++){
					window.clearPixels();
					memset(DepthArray, 0, sizeof(DepthArray));

					for (int j = 22; j < 32; j++){
						triangles[j].vertices[0].y += 0.05;
						triangles[j].vertices[1].y += 0.05;
						triangles[j].vertices[2].y += 0.05;
					}

					frameNumber = render(triangles, window, indexToFile, lightSources, 1);
				}

				float angleToChange = convertDegrees(6);

				//make the rectangle do a backflip
				for (int s=0; s<60; s++) {
					window.clearPixels();
					memset(DepthArray, 0, sizeof(DepthArray));

					for (int j = 22; j < 32; j++){
						//calculate the centroid of the cubeoid
						float topMiddleX1 = (triangles[1].vertices[0].x + triangles[1].vertices[1].x + triangles[1].vertices[2].x) / 3;
						float topMiddleY1 = (triangles[1].vertices[0].y + triangles[1].vertices[1].y + triangles[1].vertices[2].y) / 3;
						float topMiddleZ1 = (triangles[1].vertices[0].z + triangles[1].vertices[1].z + triangles[1].vertices[2].z) / 3;

						float topMiddleX2 = (triangles[6].vertices[0].x + triangles[6].vertices[1].x + triangles[6].vertices[2].x) / 3;
						float topMiddleY2 = (triangles[6].vertices[0].y + triangles[6].vertices[1].y + triangles[6].vertices[2].y) / 3;
						float topMiddleZ2 = (triangles[6].vertices[0].z + triangles[6].vertices[1].z + triangles[6].vertices[2].z) / 3;

						float topMiddleX = (topMiddleX1 + topMiddleX2)/2;
						float topMiddleY = (topMiddleY1 + topMiddleY2)/2;
						float topMiddleZ = (topMiddleZ1 + topMiddleZ2)/2;

						float botMiddleX1 = (triangles[5].vertices[0].x + triangles[5].vertices[1].x + triangles[5].vertices[2].x) / 3;
						float botMiddleY1 = (triangles[5].vertices[0].y + triangles[5].vertices[1].y + triangles[5].vertices[2].y) / 3;
						float botMiddleZ1 = (triangles[5].vertices[0].z + triangles[5].vertices[1].z + triangles[5].vertices[2].z) / 3;

						float botMiddleX2 = (triangles[10].vertices[0].x + triangles[10].vertices[1].x + triangles[10].vertices[2].x) / 3;
						float botMiddleY2 = (triangles[10].vertices[0].y + triangles[10].vertices[1].y + triangles[10].vertices[2].y) / 3;
						float botMiddleZ2 = (triangles[10].vertices[0].z + triangles[10].vertices[1].z + triangles[10].vertices[2].z) / 3;

						float botMiddleX = (botMiddleX1 + botMiddleX2)/2;
						float botMiddleY = (botMiddleY1 + botMiddleY2)/2;
						float botMiddleZ = (botMiddleZ1 + botMiddleZ2)/2;

						float centroidX = topMiddleX + botMiddleX/2;
						float centroidY = topMiddleY + botMiddleY/2;
						float centroidZ = topMiddleZ + botMiddleZ/2;

						for (int k = 0; k < 3; k++) {
							float x = triangles[j].vertices[k].x - centroidX;
							float y = triangles[j].vertices[k].y - centroidY;
							float z = triangles[j].vertices[k].z - centroidZ;

							float rotatedY = y * cos(angleToChange) - z * sin(angleToChange);
							float rotatedZ = y * sin(angleToChange) + z * cos(angleToChange);

							triangles[j].vertices[k].x = x + centroidX;
							triangles[j].vertices[k].y = rotatedY + centroidY;
							triangles[j].vertices[k].z = rotatedZ + centroidZ;
						}
					}
					
					frameNumber = render(triangles, window, indexToFile, lightSources, 1);
				}

				for (int i = 0; i < 3; i++){
					window.clearPixels();
					memset(DepthArray, 0, sizeof(DepthArray));

					for (int j = 22; j < 32; j++){
						triangles[j].vertices[0].y -= 0.05;
						triangles[j].vertices[1].y -= 0.05;
						triangles[j].vertices[2].y -= 0.05;
					}

					frameNumber = render(triangles, window, indexToFile, lightSources, 1);
				}
			}
			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_z){

				// HOW TO SET UP SEQUENCE:
				// use window.savePPM with incrementing frame count
					// window.savePPM("frame_" + std::to_string(frameNumber) + ".ppm");
					// run this after each window.renderFrame()
				// ffmpeg -framerate 25 -i frame_%d.ppm -c:v libx264 -movflags +faststart output.mp4

				//cool place to wathc the light move from
				//Vec3: (0, 1.4, 2.8)
				// Mat4:
				// (1 0 0 )
				// (0 0.838671 0.544639 )
				// (0 -0.544639 0.838671 )

				std::cout << "z down - sequence" <<std::endl;
				int frameNumber=0;

				//wireframe orbit
				renderMode = WIREFRAME;
				for (int i=0; i<36; i++){
					window.clearPixels();
					memset(DepthArray, 0, sizeof(DepthArray));
					rotateAboutOrigin(convertDegrees(10));
					lookAt(glm::vec3 (0,0,0));
					frameNumber = render(triangles, window, indexToFile, lightSources, frameNumber);
				}

				//rasterise orbit
				renderMode = RASTERISE;
				for (int i=0; i<36; i++){
					window.clearPixels();
					memset(DepthArray, 0, sizeof(DepthArray));
					rotateAboutOrigin(convertDegrees(10));
					lookAt(glm::vec3 (0,0,0));
					frameNumber = render(triangles, window, indexToFile, lightSources, frameNumber);
				}
				memset(DepthArray, 0, sizeof(DepthArray));

				//ratrace orbit
				renderMode = RAYTRACE;
				for (int i=0; i<36; i++){
					window.clearPixels();
					rotateAboutOrigin(convertDegrees(10));
					lookAt(glm::vec3 (0,0,0));
					frameNumber = render(triangles, window, indexToFile, lightSources, frameNumber);
				}
				window.clearPixels();

				USE_SKYBOX=1;

				moveSmoothly(cameraPosition, glm::vec3(0,1.4f,2.8f), cameraOrientation, glm::mat3(
						1.0f, 0.0f, 0.0f, 
						0.0f, 0.838671f, 0.544639f, 
						0.0f, -0.544639f, 0.838671f
					), 5, triangles, window, indexToFile, lightSources, frameNumber);  //destination mamtrix isnt quite right

				for (int i = 0; i < 5; i++) {
					float shift = 1.0f - i * 0.2f;
					window.clearPixels();
					std::vector<glm::vec3> lightSources = {
						glm::vec3(-0.8f * shift, 0.8f, -0.8f * shift),
						glm::vec3(0.8f * shift, 0.8f, -0.8f * shift),
						glm::vec3(-0.8f * shift, 0.8f, 0.8f * shift),
						glm::vec3(0.8f * shift, 0.8f, 0.8f * shift)
					};
					frameNumber = render(triangles, window, indexToFile, lightSources, frameNumber);
				}

				std::vector<glm::vec3> lightSources = softShadowsLightSources (glm::vec3(0,0.8,0), 0.03, 8); //try grid instead of normal
				frameNumber = render(triangles, window, indexToFile, lightSources, frameNumber);

				moveSmoothly(cameraPosition, glm::vec3(0.0, 0.0, 4.0), cameraOrientation, glm::mat3 ({1,0,0},{0,1,0},{0,0,1}),
				5, triangles, window, indexToFile, lightSources, frameNumber);

				indexToFile[26] = "mirror";
				indexToFile[31] = "mirror";
				frameNumber = render(triangles, window, indexToFile, lightSources, frameNumber);

				indexToFile[8] = "normal-map";
				indexToFile[9] = "normal-map";
				frameNumber = render(triangles, window, indexToFile, lightSources, frameNumber);

				window.clearPixels();
				METALIC_MIRROR = 0;
				frameNumber = render(triangles, window, indexToFile, lightSources, frameNumber);

				window.clearPixels();
				USE_TEXTURED_FLOOR = 1;
				indexToFile[6] = "textured-floor";
				indexToFile[7] = "textured-floor";
				frameNumber = render(triangles, window, indexToFile, lightSources, frameNumber);

				//make the box do a backslip
				//should also make it move out of the box because weird stuff is happening when it collides
				for (int i = 0; i < 3; i++){
					window.clearPixels();
					memset(DepthArray, 0, sizeof(DepthArray));

					for (int j = 22; j < 32; j++){
						triangles[j].vertices[0].y += 0.05;
						triangles[j].vertices[1].y += 0.05;
						triangles[j].vertices[2].y += 0.05;
					}

					frameNumber = render(triangles, window, indexToFile, lightSources, 1);
				}

				float angleToChange = convertDegrees(6);

				//make the rectangle do a backflip
				for (int s=0; s<60; s++) {
					window.clearPixels();
					memset(DepthArray, 0, sizeof(DepthArray));

					for (int j = 22; j < 32; j++){
						//calculate the centroid of the cubeoid
						float topMiddleX1 = (triangles[1].vertices[0].x + triangles[1].vertices[1].x + triangles[1].vertices[2].x) / 3;
						float topMiddleY1 = (triangles[1].vertices[0].y + triangles[1].vertices[1].y + triangles[1].vertices[2].y) / 3;
						float topMiddleZ1 = (triangles[1].vertices[0].z + triangles[1].vertices[1].z + triangles[1].vertices[2].z) / 3;

						float topMiddleX2 = (triangles[6].vertices[0].x + triangles[6].vertices[1].x + triangles[6].vertices[2].x) / 3;
						float topMiddleY2 = (triangles[6].vertices[0].y + triangles[6].vertices[1].y + triangles[6].vertices[2].y) / 3;
						float topMiddleZ2 = (triangles[6].vertices[0].z + triangles[6].vertices[1].z + triangles[6].vertices[2].z) / 3;

						float topMiddleX = (topMiddleX1 + topMiddleX2)/2;
						float topMiddleY = (topMiddleY1 + topMiddleY2)/2;
						float topMiddleZ = (topMiddleZ1 + topMiddleZ2)/2;

						float botMiddleX1 = (triangles[5].vertices[0].x + triangles[5].vertices[1].x + triangles[5].vertices[2].x) / 3;
						float botMiddleY1 = (triangles[5].vertices[0].y + triangles[5].vertices[1].y + triangles[5].vertices[2].y) / 3;
						float botMiddleZ1 = (triangles[5].vertices[0].z + triangles[5].vertices[1].z + triangles[5].vertices[2].z) / 3;

						float botMiddleX2 = (triangles[10].vertices[0].x + triangles[10].vertices[1].x + triangles[10].vertices[2].x) / 3;
						float botMiddleY2 = (triangles[10].vertices[0].y + triangles[10].vertices[1].y + triangles[10].vertices[2].y) / 3;
						float botMiddleZ2 = (triangles[10].vertices[0].z + triangles[10].vertices[1].z + triangles[10].vertices[2].z) / 3;

						float botMiddleX = (botMiddleX1 + botMiddleX2)/2;
						float botMiddleY = (botMiddleY1 + botMiddleY2)/2;
						float botMiddleZ = (botMiddleZ1 + botMiddleZ2)/2;

						float centroidX = topMiddleX + botMiddleX/2;
						float centroidY = topMiddleY + botMiddleY/2;
						float centroidZ = topMiddleZ + botMiddleZ/2;

						for (int k = 0; k < 3; k++) {
							float x = triangles[j].vertices[k].x - centroidX;
							float y = triangles[j].vertices[k].y - centroidY;
							float z = triangles[j].vertices[k].z - centroidZ;

							float rotatedY = y * cos(angleToChange) - z * sin(angleToChange);
							float rotatedZ = y * sin(angleToChange) + z * cos(angleToChange);

							triangles[j].vertices[k].x = x + centroidX;
							triangles[j].vertices[k].y = rotatedY + centroidY;
							triangles[j].vertices[k].z = rotatedZ + centroidZ;
						}
					}
					
					frameNumber = render(triangles, window, indexToFile, lightSources, 1);
				}

				for (int i = 0; i < 3; i++){
					window.clearPixels();
					memset(DepthArray, 0, sizeof(DepthArray));

					for (int j = 22; j < 32; j++){
						triangles[j].vertices[0].y -= 0.05;
						triangles[j].vertices[1].y -= 0.05;
						triangles[j].vertices[2].y -= 0.05;
					}

					frameNumber = render(triangles, window, indexToFile, lightSources, 1);
				}

				std::cout<<"sequence end"<<std::endl;
			}
	}
}
}