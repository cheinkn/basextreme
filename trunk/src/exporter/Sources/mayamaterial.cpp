
#define _BOOL
#include <string>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>

#include "mayamaterial.h"


MayaMaterial* ExtractShader(MFnLambertShader &rcLambert);


bool HasMaterial(std::vector<MayaMaterial*>* papcMayaMaterial, MString& rstrName)
{
	unsigned int i;
	unsigned int n = papcMayaMaterial->size();

	for (i = 0; i < n; i++) {
		if ((*papcMayaMaterial)[i]->m_strName == rstrName) {
			return true;
		}
	}

	return false;
}


void ExtractMaterials(std::vector<MayaMaterial*>* papcMayaMaterial, MObjectArray& racShaders)
{
	unsigned int i;
	unsigned int shaders = racShaders.length();

	for (i = 0; i < shaders; i++) {
		if (racShaders[i].hasFn(MFn::kShadingEngine)) {
			MFnDependencyNode node(racShaders[i]);
			MPlug plug = node.findPlug("surfaceShader");
			MPlugArray materials;

			plug.connectedTo(materials, true, false);
			unsigned int k;
			for (k = 0; k < materials.length(); k++) {
				MObject material = materials[k].node();
				if (material.hasFn(MFn::kLambert)) {
					MFnLambertShader lambert(material);
					if (HasMaterial(papcMayaMaterial, lambert.name())) {
						continue;
					}
					papcMayaMaterial->push_back(ExtractShader(lambert));
				} else {
					printf("WARNING!: Unsupported shader: %s\n", material.apiTypeStr());
				}
			}
		} else {
			printf("WARNING!: Not a MFn::kShaderEngine in shader list: %s\n", racShaders[i].apiTypeStr());
		}
	}
}


MayaMaterial* ExtractShader(MFnLambertShader &rcLambert)
{
	MayaMaterial *pcMaterial = new MayaMaterial();

	pcMaterial->m_strName = rcLambert.name();
	pcMaterial->m_strTechnique = "diffuse";
	pcMaterial->m_fRepeatU = 1.0f;
	pcMaterial->m_fRepeatV = 1.0f;

	printf("\tExtracting lambert shader: %s\n", rcLambert.name().asChar());

	/// Extract texture name & 2d mapping
	MObject cColor;
	MPlugArray cColorPlugs;
	rcLambert.findPlug("color").connectedTo(cColorPlugs, true, false);
	if (cColorPlugs.length() > 0) {
		if (cColorPlugs[0].node().hasFn(MFn::kFileTexture)) {
			MFnDependencyNode cFile(cColorPlugs[0].node());
			cFile.findPlug("fileTextureName").getValue(pcMaterial->m_strDiffuseTexture);

			cFile.findPlug("repeatU").getValue(pcMaterial->m_fRepeatU);
			cFile.findPlug("repeatV").getValue(pcMaterial->m_fRepeatV);
		} else {
			printf("WARNING: color plug is not MFn::kFileTexture: %s\n", cColorPlugs[0].node().apiTypeStr());
		}
	} else {
		printf("WARNING!: color plug is empty for shader: %s\n", rcLambert.name().asChar());
	}

	return pcMaterial;
}
