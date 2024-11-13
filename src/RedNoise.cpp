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
#define WIDTH 320
#define HEIGHT 240
float DepthArray [HEIGHT] [WIDTH] = {{0}};
glm::vec3 cameraPosition (0.0, 0.0, 4.0);
glm::mat3 cameraOrientation ({1,0,0},{0,1,0},{0,0,1});
enum renderType{WIREFRAME, RASTERISE, RAYTRACE};
renderType renderMode = WIREFRAME;
std::array<bool, 3> lightingMode = {0,0,0};
bool USE_MIRROR = 1;
bool METALIC_MIRROR = 1;
bool USE_TEXTURED_FLOOR = 1;
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
RayTriangleIntersection getClosestIntersection (glm::vec3 rayDirection, std::vector<ModelTriangle> triangles, glm::vec3 r){
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

bool findIntersectionBetweenTwoPoints(glm::vec3 source, glm::vec3 destination, std::vector<ModelTriangle> triangles) {
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

	if (1/pixel.depth > DepthArray[yInt][xInt] + 0.0001f){
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

float gouraud(RayTriangleIntersection intersectionDetails, glm::vec3 lightSource, std::vector<ModelTriangle> triangles){
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

float phong(RayTriangleIntersection intersectionDetails, glm::vec3 lightSource, std::vector<ModelTriangle> triangles){
	//commented out to allow sphere to only use angle of incidence
	// if (findIntersectionBetweenTwoPoints(intersectionDetails.intersectionPoint, lightSource, triangles)){
	// 		return 0.2;
	// }


	glm::vec3 Vertex = intersectionDetails.intersectionPoint;
	glm::vec3 viewVector = glm::normalize(cameraPosition - Vertex);
	std::array<glm::vec3,3> normals = intersectionDetails.intersectedTriangle.normals;
	std::vector<float> barycentrics = computeBarycentricPoints(intersectionDetails.intersectionPoint, intersectionDetails.intersectedTriangle);
	glm::vec3 vertexNormal = barycentrics[1]*normals[0] + barycentrics[0]*normals[1] + barycentrics[2]*normals[2];

	float intensityP = proximityLighting(Vertex, lightSource, 5);
	float intensityA = aoiLighting(Vertex, lightSource, vertexNormal, 10);
	float intensityS = 2*specularLighting(Vertex, lightSource, vertexNormal, viewVector, 256);

    intensityP = glm::clamp(intensityP, 0.0f, 1.0f);
    intensityA = glm::clamp(intensityA, 0.0f, 1.0f);
    intensityS = glm::clamp(intensityS, 0.0f, 1.0f);

	//could fiddle about with this
	float intensity = 0.2+(intensityP*intensityA)+(intensityS*intensityA);
	if (intensity>1) return 1;	

	return intensity;
}

float genericShading(RayTriangleIntersection intersectionDetails, glm::vec3 lightSource, std::vector<ModelTriangle> triangles){
	glm::vec3 Vertex = intersectionDetails.intersectionPoint;
	glm::vec3 VertexNormal = intersectionDetails.intersectedTriangle.normal;
	glm::vec3 viewVector = glm::normalize(cameraPosition - Vertex);

	float intensity;
	if (findIntersectionBetweenTwoPoints(intersectionDetails.intersectionPoint, lightSource, triangles)){
		intensity = 0.2;
	}
	else{
		float intensityP = 1.7*proximityLighting(Vertex, lightSource, 5);
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

Colour texture3D(RayTriangleIntersection intersectionDetails, TextureMap texture, std::vector<uint32_t> pixels){
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

	int textureXdistance = round(u*texture.width);
	int textureYdistance = round(v*texture.height);
	//grab pixel
	uint32_t pixel = texture.pixels[textureYdistance * texture.width + textureXdistance];
	//convert to RGB format so it can be manipulated by intensity later
	uint8_t r = (pixel >> 16) & 0xFF;
	uint8_t g = (pixel >> 8) & 0xFF;
	uint8_t b = pixel & 0xFF;

	return Colour(r,g,b);
}

void drawRayTraced(std::vector<ModelTriangle> triangles, DrawingWindow &window, std::unordered_map<int, std::string> indexToFile) {
    int focalLength = 2;
	// glm::vec3 lightSource(-0.8,0.9,-0.8); //back-left
	// glm::vec3 lightSource(0.8,0.9,-0.8); //back-right
	// glm::vec3 lightSource(-0.8,0.9,0.8); //front-left
	// glm::vec3 lightSource(0.8,0.9,0.8); //front-right
	// glm::vec3 lightSource(0,0.9,0); //middle

	//bl, br, fl, fr
	std::array<glm::vec3,4> lightSources = {(-0.8,0.9,-0.8),(0.8,0.9,-0.8),(-0.8,0.9,0.8),(0.8,0.9,0.8)};

	TextureMap texture = TextureMap("./assets/texture2.ppm"); //width: 480, height: 395
	std::vector<uint32_t> pixels = texture.pixels;

    for (size_t y = 0; y < HEIGHT; y++) {
        for (size_t x = 0; x < WIDTH; x++) {
            float u = (x - WIDTH / 2.0f) / (WIDTH / focalLength);
            float v = -(y - HEIGHT / 2.0f) / (WIDTH / focalLength);

            glm::vec3 pixelInCam = glm::vec3(u, v, -focalLength);
            glm::vec3 rayDirection = glm::normalize(pixelInCam * cameraOrientation);

            RayTriangleIntersection intersectionDetails = getClosestIntersection(rayDirection, triangles, cameraPosition);

			//occlusion check (skip if no intersection)
            if (intersectionDetails.distanceFromCamera == -1) {continue;} 

			Colour oldColour;
			float intensity;
			if (indexToFile[intersectionDetails.triangleIndex]=="cornell-box"){
				intensity = genericShading(intersectionDetails, lightSource, triangles);
				oldColour = intersectionDetails.intersectedTriangle.colour;
				
			} else if (indexToFile[intersectionDetails.triangleIndex]=="sphere"){
				intensity = phong(intersectionDetails, lightSource, triangles);
				oldColour = intersectionDetails.intersectedTriangle.colour;

			} else if (indexToFile[intersectionDetails.triangleIndex]=="textured-floor"){
				intensity = genericShading(intersectionDetails, lightSource, triangles);
				oldColour = texture3D(intersectionDetails, texture, pixels);

			} else if (indexToFile[intersectionDetails.triangleIndex]=="mirror"){
				if (USE_MIRROR == 1){
					glm::vec3 normal =intersectionDetails.intersectedTriangle.normal;
					if (METALIC_MIRROR == 1){
						normal = randomlyChange(intersectionDetails.intersectedTriangle.normal,1);
					}
					glm::vec3 reflectedRay = glm::normalize(rayDirection - 2.0f * glm::dot(rayDirection, normal) * normal);
					glm::vec3 intPoint = glm::vec3(intersectionDetails.intersectionPoint[0]+0.01,intersectionDetails.intersectionPoint[1]+0.01,intersectionDetails.intersectionPoint[2]+0.01);
					intersectionDetails = getClosestIntersection(reflectedRay, triangles, intPoint);
					if (indexToFile[intersectionDetails.triangleIndex]=="textured-floor"){
						oldColour = texture3D(intersectionDetails, texture, pixels);
					} else{
						oldColour = intersectionDetails.intersectedTriangle.colour;
					}
				} else{
					oldColour = intersectionDetails.intersectedTriangle.colour;
				}
				intensity = genericShading(intersectionDetails, lightSource, triangles);

			}else{
				intensity = 0;
				std::cout<<"sticky one still, check for indexToFile error"<<std::endl;
			}

			Colour newColour (oldColour.red*intensity, oldColour.green*intensity, oldColour.blue*intensity);
			window.setPixelColour(x, y, convertColour(newColour));
    
        }
    }	
}

void draw3DTriangles(std::vector<ModelTriangle> triangles,  DrawingWindow &window){
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

void drawWireframe(std::vector<ModelTriangle> triangles,  DrawingWindow &window){
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

void render(std::vector<ModelTriangle> triangles,  DrawingWindow &window, std::unordered_map<int, std::string> indexToFile){
	if (renderMode==WIREFRAME){
		drawWireframe(triangles,window);
	} else if (renderMode==RASTERISE){
		draw3DTriangles(triangles, window);
	} else if (renderMode==RAYTRACE){
		drawRayTraced(triangles, window, indexToFile);
	} else{
		std::cout<<"render type unknown"<<std::endl;
	}
	// std::cout<<"render completed"<<std::endl;
}

int main(int argc, char *argv[]) {
	DrawingWindow window = DrawingWindow(WIDTH, HEIGHT, false);
	SDL_Event event;

	//given a triangle index, will reveal which file it came from
	std::unordered_map<int, std::string> indexToFile;

	std::tuple<std::vector<Colour>, std::vector<std::string>, std::vector<std::string>> colours = MTLparser ("./assets/textured-cornell-box.mtl");
	//returns texture files to use (3rd item in the tuple) not actually being used, am hard coding


	//have hardcoded texture file to remove instances of cobbles (replaced with green)
	std::vector<ModelTriangle> trianglesCornelBox = OBJparser ("/home/greg/Documents/CG2024/lab7_phongCompleted/assets/textured-cornell-box.obj", colours, 0.35, glm::vec3(0,0,0));
	for (int i=0; i<trianglesCornelBox.size(); i++){
		indexToFile[i] = "cornell-box";
	}
	std::vector<ModelTriangle> triangles = trianglesCornelBox;
	std::vector<ModelTriangle> trianglesSphere = OBJparser ("/home/greg/Documents/CG2024/lab7_phongCompleted/assets/sphere.obj", colours, 0.35, glm::vec3(0.75,-0.2,-0.5));
	for (int i=triangles.size(); i<triangles.size()+trianglesSphere.size(); i++){
		indexToFile[i] = "sphere";
	}
	triangles.insert(triangles.end(), trianglesSphere.begin(), trianglesSphere.end());



	//hardcodes front of blue box to be a mirror
	indexToFile[26] = "mirror";
	indexToFile[31] = "mirror";

	//hardcodes the floor to be textured
	indexToFile[6] = "textured-floor";
	indexToFile[7] = "textured-floor";

	glm::vec3 startingCamera (0.0, 0.0, 4.0);
	cameraPosition = startingCamera;	
	bool orbit = 0;

	while (true) {

		window.clearPixels();
		memset(DepthArray, 0, sizeof(DepthArray));
		if (orbit==1){
			rotateAboutOrigin(convertDegrees(1));
			lookAt(glm::vec3 (0,0,0));	
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
			// if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_g){
			// 	std::cout << "rotate about origin" << std::endl;
			// 	rotateAboutOrigin (convertDegrees(90));
			// 	std::cout << "camera location" << cameraPosition[0] << "," 
			// 	<< cameraPosition[1] << ","
			// 	<< cameraPosition[2] <<
			// 	std::endl;			
			// }
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
			}
			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_m){
				std::cout << "m down - raytrace" << std::endl;
				renderMode = RAYTRACE;
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
			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_l){
				std::cout << "l down - specular lighting toggle" << std::endl;
				std::cout <<lightingMode[0]<<lightingMode[1]<<lightingMode[2]<<std::endl;
				lightingMode[2] = abs(lightingMode[2]-1);
			}
		render(triangles, window, indexToFile);
		window.renderFrame();
	}


}
}