/*---------------------------------------------------------------------------*\
License
  This file is part of AccelerateCFD_Community_Edition.

  AccelerateCFD_Community_Edition is free software: you can redistribute it 
  and/or modify it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or (at your option) 
  any later version.

  AccelerateCFD_Community_Edition is distributed in the hope that it will be useful, 
  but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with AccelerateCFD_Community_Edition.  If not, see <http://www.gnu.org/licenses/>.

Application
  podPostProcess

Description
  This application allows users to obtain additional information from reduced order
  as well as full order models for comparison and reference purposes. Right now this 
  utility supports calculation of time varying coefficients from full order model
  that can serve as a reference to reduced order time coefficients calculated using 
  podROM utility. This utility operates based on command line arguments. All available 
  arguments are explained in README file.

Author
  Illinois Rocstar LLC
  AccelerateCFD Development Team
  Copyright (C) 2017-2019

\*---------------------------------------------------------------------------*/

#include "argList.H"
#include "timeSelector.H"
#include "ReadFields.H"
#include "volFields.H"
#include "IFstream.H"
#include "OFstream.H"
#include <vector>

using namespace Foam;

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

void copyrightnotice()
{
  std::cout << 
  "*******************************************************************************" 
  << std::endl;

  std::cout << 
  "*******************************************************************************" 
  << std::endl;

  std::cout << "* AccelerateCFD_Community_Edition" << std::endl;
  std::cout << "* Copyright (C) 2017-2019 Illinois Rocstar LLC" << std::endl;
  std::cout << "* GNU GENERAL PUBLIC LICENSE VERSION 3 (2007)" << std::endl;
  std::cout << "* www.Illinoisrocstar.com" << std::endl;

  std::cout << 
  "*******************************************************************************" 
  << std::endl;

  std::cout << 
  "*******************************************************************************" 
  << std::endl;
}

// inner product of 2 vectors
double innerProductPOD(volVectorField v1, volVectorField v2, volScalarField cellVols)
{
  return gSum(volScalarField((v1&v2)*cellVols));
}

// inner product of 2 tensors (double dot product)
double innerProductPOD2(volTensorField v1, volSymmTensorField v2, volScalarField cellVols)
{
  return gSum(volScalarField((v1&&v2)*cellVols));
}

int main(int argc, char *argv[])
{
    copyrightnotice();

  // To get processor clocktime
  std::clock_t start;
  double duration;
  start = std::clock();
  argList::validOptions.clear();

  argList::validArgs.append("get_aPOD");

  Foam::argList args(argc, argv);

  std::vector<string> argsVec;
  for (int i=0; i<argc; i++)
    argsVec.push_back(argv[i]);

  bool enable_aPOD = false;

  if ((argsVec.size() > 1) && (argsVec[1] == "get_aPOD"))
    enable_aPOD = true;

  if (enable_aPOD) {

  #include "createTime.H"
  #include "createNamedMesh.H"

  instantList timeDirs = timeSelector::select0(runTime, args);

  // Reads user-defined data from podDict in constant directory
  IOdictionary podDict
  (
    IOobject
    (
      "podDict",
      runTime.constant(),
      mesh,
      IOobject::MUST_READ,
      IOobject::NO_WRITE
    )
  );

  dimensionedScalar nDim_ds
  (
  podDict.lookup("nDim")
  );
  dimensionedScalar nu_ds
  (
  podDict.lookup("nu")
  );
  dimensionedScalar writeFreq_ds
  (
  podDict.lookup("writeFreq")
  );
  dimensionedScalar tEnd_ds
  (
  podDict.lookup("tEnd")
  );
  dimensionedScalar dt_ds
  (
  podDict.lookup("dt")
  );
  dimensionedScalar closure_nu
  (
  podDict.lookup("artificial_nu")
  );

  int nDim = (int) nDim_ds.value(); //# of modes used
  double nu = (double) nu_ds.value(); //nu value
  int writeFreq = (int) writeFreq_ds.value(); //Closure model update frequency
  double tEnd = (double) tEnd_ds.value(); //closure model runtime end
  double dt = (double) dt_ds.value(); //FDM time step
  double nu_tilda = (double) closure_nu.value(); //closure viscosity

  
  runTime.setTime(timeDirs.last(),0);
    
  volScalarField cellVolume
  (
    IOobject
    (
	    "cellVolume",
	    runTime.timeName(),
	    mesh,
	    IOobject::NO_READ,
	    IOobject::NO_WRITE
    ),
    mesh,
    dimensionedScalar("zero",dimVolume,0.0)
  );
  cellVolume.ref() = mesh.V();

  std::vector<volVectorField> sigmas;
  std::string sigmaName;

  for (int iSig=0; iSig<nDim; iSig++){
    sigmaName = "sigma_" + std::to_string(iSig); //skip sigma_0
    sigmas.push_back(
      volVectorField
      (
        IOobject
        (
          sigmaName,
          runTime.timeName(),
          mesh,
          IOobject::MUST_READ,
          IOobject::NO_WRITE
        ),
        mesh
      )
    );
  }

  volVectorField meanFlow =
    volVectorField
    (
      IOobject
      (
        "UMean",
        runTime.timeName(),
        mesh,
        IOobject::MUST_READ,
        IOobject::NO_WRITE
      ),
      mesh
    );

    List<scalar> aList(nDim); // storing velocity mode coefficients

    ofstream avals;
    avals.open ("aPOD.csv");

    forAll(timeDirs, timei)
    {
        runTime.setTime(timeDirs[timei], timei);
        scalar time=runTime.value(); 
        volVectorField U
        (
            IOobject
            (
                "U",
                runTime.timeName(),
                mesh,
                IOobject::MUST_READ,
                IOobject::NO_WRITE
            ),
            mesh
        );
       
        volVectorField UPrime = U - meanFlow;
        forAll(aList,i)
        {
            scalar& s = aList[i];
            s = innerProductPOD(sigmas[i],UPrime,cellVolume); // calculate a_0 from initial velocity fluctuation field
        }
        avals << time << ",";
        for (int i=0; i<nDim; i++){
            avals << aList[i] << ",";
        }
        avals << nl << std::flush;
    }

    avals.close();
  }
  else {
    std::cerr << "Please select which post process function you would want to use"
    << std::endl;
    std::cerr << " ""podPostProcess -help"" to see all available options" << std::endl;
    throw;
  }
  
    return 0;
}


// ************************************************************************* //
