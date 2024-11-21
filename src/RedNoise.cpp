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
renderType renderMode = RASTERISE;
std::array<bool, 3> lightingMode = {0,0,0};
bool USE_MIRROR = 1;
bool METALIC_MIRROR = 0;
bool USE_TEXTURED_FLOOR = 1;
bool USE_SKYBOX = 1;
bool USE_NORMAL_MAP = 1;
bool GDB_FILES = 1;
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
	//commented out to allow sphere to only use angle of incidence
	// if (findIntersectionBetweenTwoPoints(intersectionDetails.intersectionPoint, lightSource, triangles)){
	// 		return 0.2;
	// }

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

	// if (intersectionDetails.triangleIndex!=12 || intersectionDetails.triangleIndex!=17){
	// 	std::cout<<VertexNormal[0]<<","<<VertexNormal[1]<<","<<VertexNormal[2]<<std::endl;
	// }

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


	// if (intersectionDetails.triangleIndex!=12 || intersectionDetails.triangleIndex!=17){
	// 	std::cout<<VertexNormal[0]<<","<<VertexNormal[1]<<","<<VertexNormal[2]<<std::endl;
	// }

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
	if (USE_TEXTURED_FLOOR==0){
		return intersectionDetails.intersectedTriangle.colour;
	}
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

Colour vertexNormalFinder(RayTriangleIntersection intersectionDetails, TextureMap texture, std::vector<uint32_t> pixels){
	// if (USE_TEXTURED_FLOOR==0){ //do a vertex normal equivalent
	// 	return intersectionDetails.intersectedTriangle.colour;
	// }
	std::vector<float> barycentrics = computeBarycentricPoints(intersectionDetails.intersectionPoint,intersectionDetails.intersectedTriangle);

	float u = barycentrics[1]*intersectionDetails.intersectedTriangle.vertices[0].x+
				barycentrics[0]*intersectionDetails.intersectedTriangle.vertices[1].x+
				barycentrics[2]*intersectionDetails.intersectedTriangle.vertices[2].x;

	float v = barycentrics[1]*intersectionDetails.intersectedTriangle.vertices[0].y+
				barycentrics[0]*intersectionDetails.intersectedTriangle.vertices[1].y+
				barycentrics[2]*intersectionDetails.intersectedTriangle.vertices[2].y;

	int textureXdistance = round(u*texture.width);
	int textureYdistance = round(v*texture.height);
	//grab pixel
	uint32_t pixel = texture.pixels[textureYdistance * texture.width + textureXdistance];
	//convert to RGB format so it can be manipulated by intensity later
	uint8_t r = (pixel >> 16) & 0xFF;
	uint8_t g = (pixel >> 8) & 0xFF;
	uint8_t b = pixel & 0xFF;

	//prints values for the textured top
	// if (intersectionDetails.triangleIndex!=6 && intersectionDetails.triangleIndex!=7){
	// 	std::cout<<intersectionDetails.triangleIndex<<","<<Colour(r,g,b)<<std::endl;
	// 	std::cout<<"intersection point: "<<intersectionDetails.intersectionPoint[0]<<","<<intersectionDetails.intersectionPoint[1]<<","<<intersectionDetails.intersectionPoint[2]<<std::endl;
	// 	std::cout << "Texture X: " << textureXdistance << ", Y: " << textureYdistance << std::endl;
	// 	std::cout << "u:" <<u<<"v: "<<v<<std::endl;
	// }
	return Colour(r,g,b);
}

// uint32_t getSkyboxPixel(glm::vec3 rayDirection, TextureMap &backTexture, TextureMap &bottomTexture, TextureMap &frontTexture, TextureMap &leftTexture, TextureMap &rightTexture, TextureMap &topTexture){

// 	float dominantDirection = std::fmax(std::fmax(abs(rayDirection[0]), abs(rayDirection[1])), abs(rayDirection[2]));
// 	int u=0;
// 	int v=0;

//     int pixels_width = backTexture.width;
//     int pixels_height = backTexture.height;

// 	if (dominantDirection == abs(rayDirection[0])){ //left or right
// 		if (rayDirection[0]>0){
// 			u = round((-rayDirection[2] / abs(rayDirection[0]) + 1.0f) * 0.5f * (pixels_width - 1));
// 			v = round((-rayDirection[1] / abs(rayDirection[0]) + 1.0f) * 0.5f * (pixels_height - 1));
//             return rightTexture.pixels[v * pixels_width + u];
// 		} else{
// 			u = round((rayDirection[2] / abs(rayDirection[0]) + 1.0f) * 0.5f * (pixels_width - 1));
// 			v = round((-rayDirection[1] / abs(rayDirection[0]) + 1.0f) * 0.5f * (pixels_height - 1));
//             return leftTexture.pixels[v * pixels_width + u];
// 		}
// 	} else if (dominantDirection == abs(rayDirection[1])){ //top or bottom
// 		if (rayDirection[1]>0){ //top
// 			u = round((rayDirection[0] / abs(rayDirection[1]) + 1.0f) * 0.5f * (pixels_width - 1));
// 			v = round((rayDirection[2] / abs(rayDirection[1]) + 1.0f) * 0.5f * (pixels_height - 1));
//             return topTexture.pixels[v * pixels_width + u];
// 		} else{ //bottom
// 			u = round((rayDirection[0] / abs(rayDirection[1]) + 1.0f) * 0.5f * (pixels_width - 1));
// 			v = round((-rayDirection[2] / abs(rayDirection[1]) + 1.0f) * 0.5f * (pixels_height - 1));
//             return bottomTexture.pixels[v * pixels_width + u];
// 		}
// 	}else if (dominantDirection == abs(rayDirection[2])){ //front or back
// 		if (rayDirection[2]>0){
// 			u = round((rayDirection[0] / abs(rayDirection[2]) + 1.0f) * 0.5f * (pixels_width - 1));
// 			v = round((-rayDirection[1] / abs(rayDirection[2]) + 1.0f) * 0.5f * (pixels_height - 1));
//             return frontTexture.pixels[v * pixels_width + u];
// 		} else{
// 			u = round((-rayDirection[0] / abs(rayDirection[2]) + 1.0f) * 0.5f * (pixels_width - 1));
// 			v = round((-rayDirection[1] / abs(rayDirection[2]) + 1.0f) * 0.5f * (pixels_height - 1));
//             return backTexture.pixels[v * pixels_width + u];
// 		}
// 	} else{
// 		std::cout<<"fatal skybox error!"<<std::endl;
// 		std::cout<<rayDirection[0]<<std::endl;
// 		std::cout<<rayDirection[1]<<std::endl;
// 		std::cout<<rayDirection[2]<<std::endl;
// 		std::cout<<dominantDirection<<std::endl;
// 		return 0;
// 	}
// }

class Skybox {
public:
    // Constructor that takes TextureMap objects as arguments
    Skybox(TextureMap& texture, TextureMap& backTexture, TextureMap& bottomTexture,
           TextureMap& frontTexture, TextureMap& leftTexture, TextureMap& rightTexture, 
           TextureMap& topTexture, TextureMap& normalMap)
        : texture(texture), backTexture(backTexture), bottomTexture(bottomTexture),
          frontTexture(frontTexture), leftTexture(leftTexture), rightTexture(rightTexture), 
          topTexture(topTexture), normalMap(normalMap) {}

    // Function to get the pixel from the skybox
    uint32_t getSkyboxPixel(glm::vec3 rayDirection) {
        float dominantDirection = std::fmax(std::fmax(abs(rayDirection[0]), abs(rayDirection[1])), abs(rayDirection[2]));
        int u = 0;
        int v = 0;

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
    TextureMap texture;
    TextureMap backTexture;
    TextureMap bottomTexture;
    TextureMap frontTexture;
    TextureMap leftTexture;
    TextureMap rightTexture;
    TextureMap topTexture;
    TextureMap normalMap;
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


glm::vec3 refractRay(const glm::vec3& incidentRay, const glm::vec3& normal, float refractiveIndex) {
    float cosi = glm::dot(incidentRay, normal);
    float eta = 1.0f / refractiveIndex;  // Assuming air's refractive index is 1.0

    // If the ray is coming from the inside (refracting from the glass to air)
    if (cosi > 0) {
        eta = refractiveIndex;
        cosi = -cosi;
    }

    // Compute the sine of the refraction angle
    float k = 1.0f - eta * eta * (1.0f - cosi * cosi);
    if (k < 0) {
        // Total internal reflection
        return glm::vec3(0.0f);  // We'll handle reflection elsewhere
    }

    // Refract the ray
    glm::vec3 refractedRay = eta * incidentRay + (eta * cosi - sqrt(k)) * normal;
    return glm::normalize(refractedRay);
}

float fresnel(const glm::vec3& incidentRay, const glm::vec3& normal, float refractiveIndex) {
    float cosi = glm::dot(incidentRay, normal);
    float eta = refractiveIndex;
    if (cosi > 0) {
        eta = 1.0f / refractiveIndex;
    }

    float sintheta2 = eta * eta * (1.0f - cosi * cosi);
    if (sintheta2 > 1.0f) {
        // Total internal reflection
        return 1.0f;
    }

    float costheta = sqrt(1.0f - sintheta2);
    cosi = fabs(cosi);

    // Fresnel equation (Schlick's approximation)
    float r0 = (1.0f - eta) / (1.0f + eta);
    r0 = r0 * r0;
    return r0 + (1.0f - r0) * pow(1.0f - costheta, 5.0f);
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

std::tuple<float, Colour, glm::vec3> shootRay(std::vector<ModelTriangle> &triangles, std::unordered_map<int, std::string> &indexToFile, glm::vec3 rayDirection, std::vector<glm::vec3> &lightSources, RayTriangleIntersection intersectionDetails, TextureMap &texture, TextureMap &normalMap, int redirectCount, Skybox &skybox){

	Colour oldColour = Colour (255,255,255);
	float intensity;
	int maxRedirects = 20;
	redirectCount=redirectCount+1;

	if (redirectCount>maxRedirects){ //ray direction could be wrong?
		uint32_t skyboxColour = skybox.getSkyboxPixel(rayDirection); //segfalt being caused here
		return {1, uint32ToColour(skyboxColour), glm::normalize(rayDirection)};
	}

	//need to properly implement skybox inside this function
	if (intersectionDetails.distanceFromCamera==-1){
		uint32_t skyboxColour = skybox.getSkyboxPixel(rayDirection);//segfalt being caused here
		return {1, uint32ToColour(skyboxColour), glm::normalize(rayDirection)};
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
		float reflectionCoefficient = glm::clamp(fresnel(rayDirection, normal, 1.5), 0.0f, 1.0f);

		glm::vec3 intPoint = intersectionDetails.intersectionPoint + glm::vec3(0.01f);

		glm::vec3 refractedRay = glm::normalize(refractRay(rayDirection, normal, 1.5f));
		glm::vec3 reflectedRay = glm::normalize(rayDirection - 2.0f * glm::dot(rayDirection, normal) * normal);

		intersectionDetails = getClosestIntersection(refractedRay, triangles, intPoint);
		std::tuple<float, Colour, glm::vec3> refrPair = shootRay(triangles, indexToFile, refractedRay, lightSources, intersectionDetails, texture, normalMap, redirectCount, skybox);

		intersectionDetails = getClosestIntersection(reflectedRay, triangles, intPoint);
		std::tuple<float, Colour, glm::vec3> reflPair = shootRay(triangles, indexToFile, reflectedRay, lightSources, intersectionDetails, texture, normalMap, redirectCount, skybox);
		//magic number jank?


		float intensity1 = std::get<0>(refrPair);
		Colour oldColour1 = std::get<1>(refrPair);

		float intensity2 = std::get<0>(reflPair);
		Colour oldColour2 = std::get<1>(reflPair);

		if (intensity1 > 1){
			std::cout<<"magic number "<<intensity1<<std::endl;
		}
		if (intensity2 > 1){
			std::cout<<"magic number "<<intensity2<<std::endl;
		}

		//something going wrong in these last few lines here
		intensity = reflectionCoefficient * intensity2 + (1.0f - reflectionCoefficient) * intensity1;
		intensity =1;
		oldColour = combineColours(reflectionCoefficient, oldColour1, oldColour2);

	} else if (indexToFile[intersectionDetails.triangleIndex]=="normal-map"){
		Colour vertexNormalAsColour = texture3D(intersectionDetails, normalMap);
		glm::vec3 vertexNormal = convertToNormalVector(vertexNormalAsColour);
		vertexNormal = glm::normalize(vertexNormal);
		intensity = genericShading(intersectionDetails, lightSources, triangles, -vertexNormal);
		oldColour = intersectionDetails.intersectedTriangle.colour;
		redirectCount=maxRedirects;

	} else if (indexToFile[intersectionDetails.triangleIndex]=="mirror"){
		glm::vec3 normal =intersectionDetails.intersectedTriangle.normal;
		if (METALIC_MIRROR == 1){
			normal = randomlyChange(intersectionDetails.intersectedTriangle.normal,1);
		}
		rayDirection = glm::normalize(rayDirection - 2.0f * glm::dot(rayDirection, normal) * normal);
		glm::vec3 intPoint = glm::vec3(intersectionDetails.intersectionPoint[0]+0.01,intersectionDetails.intersectionPoint[1]+0.01,intersectionDetails.intersectionPoint[2]+0.01);
		intersectionDetails = getClosestIntersection(rayDirection, triangles, intPoint);
		auto bounced = shootRay(triangles, indexToFile, rayDirection, lightSources, intersectionDetails, texture, normalMap, redirectCount,skybox);


		intensity = std::get<0>(bounced);
		oldColour = std::get<1>(bounced);


	}else{
		oldColour = Colour (255,255,255);
		intensity = 0;
		std::cout<<"sticky one still, check for indexToFile error"<<std::endl;
	}
	return {intensity, oldColour, glm::normalize(rayDirection)};
}

void drawRayTraced (int startY, int endY, std::vector<ModelTriangle> &triangles, DrawingWindow &window, std::unordered_map<int, std::string> &indexToFile, std::vector<glm::vec3> &lightSources) {    
	TextureMap texture = GDB_FILES ? TextureMap("../assets/texture2.ppm") : TextureMap("./assets/texture2.ppm");
	TextureMap backTexture = GDB_FILES ? TextureMap("../assets/skybox/back.ppm") : TextureMap("./assets/skybox/back.ppm");
	TextureMap bottomTexture = GDB_FILES ? TextureMap("../assets/skybox/bottom.ppm") : TextureMap("./assets/skybox/bottom.ppm");
	TextureMap frontTexture = GDB_FILES ? TextureMap("../assets/skybox/front.ppm") : TextureMap("./assets/skybox/front.ppm");
	TextureMap leftTexture = GDB_FILES ? TextureMap("../assets/skybox/left.ppm") : TextureMap("./assets/skybox/left.ppm");
	TextureMap rightTexture = GDB_FILES ? TextureMap("../assets/skybox/right.ppm") : TextureMap("./assets/skybox/right.ppm");
	TextureMap topTexture = GDB_FILES ? TextureMap("../assets/skybox/top.ppm") : TextureMap("./assets/skybox/top.ppm");
	TextureMap normalMap = GDB_FILES ? TextureMap("../assets/normalMap2.ppm") : TextureMap("./assets/normalMap2.ppm");

	//initilise skybox object
	Skybox skybox(texture, backTexture, bottomTexture, frontTexture, leftTexture, rightTexture, topTexture, normalMap);

	std::cout<<"skybox loaded"<<std::endl;


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
				std::tuple<float, Colour, glm::vec3> result = shootRay(triangles, indexToFile, rayDirection, lightSources, intersectionDetails, texture, normalMap, 0, skybox);

				float intensity = std::get<0>(result);
				Colour oldColour = std::get<1>(result);
				glm::vec3 newRay = std::get<2>(result);

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
    const int numThreads = 4;  // Number of threads you want to use
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

int main(int argc, char *argv[]) {
	DrawingWindow window = DrawingWindow(WIDTH, HEIGHT, false);
	SDL_Event event;

	//given a triangle index, will reveal which file it came from
	std::unordered_map<int, std::string> indexToFile;

	std::tuple<std::vector<Colour>, std::vector<std::string>, std::vector<std::string>> colours = MTLparser ("../assets/textured-cornell-box.mtl");

	//returns texture files to use (3rd item in the tuple) not actually being used, am hard coding


	//have hardcoded texture file to remove instances of cobbles (replaced with green)
	std::vector<ModelTriangle> trianglesCornelBox = OBJparser ("../assets/textured-cornell-box.obj", colours, 0.35, glm::vec3(0,0,0));
	for (int i=0; i<trianglesCornelBox.size(); i++){
		indexToFile[i] = "cornell-box";
	}
	std::vector<ModelTriangle> triangles = trianglesCornelBox;
	std::vector<ModelTriangle> trianglesSphere = OBJparser ("../assets/sphere.obj", colours, 0.35, glm::vec3(0.6,0.1,-0.8));
	for (int i=triangles.size(); i<triangles.size()+trianglesSphere.size(); i++){
		indexToFile[i] = "sphere";
	}
	triangles.insert(triangles.end(), trianglesSphere.begin(), trianglesSphere.end());

	std::cout<<triangles.size()<<std::endl;


	//blue box is indexes 22-31

	//hardcodes front of blue box to be a mirror
	indexToFile[26] = "mirror";
	indexToFile[31] = "mirror";

	//hardcodes the floor to be textured
	indexToFile[6] = "textured-floor";
	indexToFile[7] = "textured-floor";

	//hardcodes the top of the red box to be normal map
	// indexToFile[12] = "normal-map";
	// indexToFile[17] = "normal-map";

	//hardcodes left wall to be normal map
	indexToFile[8] = "normal-map";
	indexToFile[9] = "normal-map";

	// hardcodes red box to be glass
    for (int i = 12; i <= 21; ++i) {
        indexToFile[i] = "glass";
    }

	glm::vec3 startingCamera (0.0, 0.0, 4.0);
	cameraPosition = startingCamera;	
	bool orbit = 0;

	int frameNumber = 0;


	//far corners
	//bl, br, fl, fr
	

	// std::vector<glm::vec3> lightSources = softShadowsLightSources (glm::vec3(0,0.8,0), 0.03, 4);
	std::vector<glm::vec3> lightSources = {glm::vec3(0,0.8,0)};

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
			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_p){
				std::cout << "look at origin" << std::endl;
				lookAt(glm::vec3 (0,0,0));
			}
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
			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_j){
				std::cout << "j down - proximity lighting toggle" << std::endl;
				std::cout <<lightingMode[0]<<lightingMode[1]<<lightingMode[2]<<std::endl;
				lightingMode[0] = abs(lightingMode[0]-1);
			}
			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_k){
				std::cout << "k down - area of incidence lighting toggle" << std::endl;
				std::cout <<lightingMode[0]<<lightingMode[1]<<lightingMode[2]<<std::endl;
				lightingMode[1] = abs(lightingMode[1]-1);
			}
			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_r){
				render(triangles, window, indexToFile, lightSources,0);
				window.renderFrame();
			}
			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_l){
				std::cout<<"l down - switch lightsource"<<std::endl;
				if (lightSources.size()>1){
						lightSources =  {
							glm::vec3(0, 0.9f, 0)};
				} else{
						lightSources =  {
							glm::vec3(0, 0.9f, 0),
							glm::vec3(0, 0.9f, 0),
							glm::vec3(0, 0.9f, 0),
							glm::vec3(0, 0.9f, 0)
						};
				}
				render(triangles, window, indexToFile, lightSources,0);
				window.renderFrame();
				std::cout<<"light source switched"<<std::endl;

			}
			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_h){
				std::cout<<"h down moving smoothly"<<std::endl;
				moveSmoothly(glm::vec3(0.0, 0.0, 4.0), glm::vec3(0.0, 0.0, 0), glm::mat3 ({1,0,0},{0,1,0},{0,0,1}), glm::mat3 ({0, 0, -1}, {0, 1, 0}, {1, 0, 0}), 20,
				triangles, window, indexToFile, lightSources, 1); //these added so that rendering can be done within the functiuon
			};

			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_f){
				std::cout << "f down, move up and then do a backflip" << std::endl;

				//move rectangle up
				for (int i = 0; i < 5; i++){
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

				for (int i = 0; i < 5; i++){
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



				std::cout << "z down - sequence" <<std::endl;
				int frameNumber=0;

				// //wireframe orbit
				for (int i=0; i<36; i++){
					window.clearPixels();
					memset(DepthArray, 0, sizeof(DepthArray));
					rotateAboutOrigin(convertDegrees(10));
					lookAt(glm::vec3 (0,0,0));
					frameNumber = render(triangles, window, indexToFile, lightSources, frameNumber);
				}

				// //rasterise orbit
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
				USE_MIRROR = 1;
				frameNumber = render(triangles, window, indexToFile, lightSources, frameNumber);

				window.clearPixels();
				METALIC_MIRROR = 1;
				frameNumber = render(triangles, window, indexToFile, lightSources, frameNumber);

				window.clearPixels();
				USE_TEXTURED_FLOOR = 1;
				frameNumber = render(triangles, window, indexToFile, lightSources, frameNumber);
				frameNumber = render(triangles, window, indexToFile, lightSources, frameNumber);


				for (int i = 0; i < 5; i++) {
					float shift = 1.0f - i * 0.2f;
					window.clearPixels();
					std::vector<glm::vec3> lightSources = {
						glm::vec3(-0.8f * shift, 0.9f, -0.8f * shift),
						glm::vec3(0.8f * shift, 0.9f, -0.8f * shift),
						glm::vec3(-0.8f * shift, 0.9f, 0.8f * shift),
						glm::vec3(0.8f * shift, 0.9f, 0.8f * shift)
					};
					frameNumber = render(triangles, window, indexToFile, lightSources, frameNumber);
				}

				std::cout<<"sequence end"<<std::endl;
			}
	}
}
}