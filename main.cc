// -*- coding: utf-8; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-

#include <petsc.h>
#include <TopOpt.h>
#include <LinearElasticity.h>
#include <MMA.h>
#include <OC.h>
#include <Filter.h>
#include <MPIIO.h>
#include <mpi.h>
/*
Authors: Niels Aage, Erik Andreassen, Boyan Lazarov, August 2013

Disclaimer:                                                              
The authors reserves all rights but does not guaranty that the code is   
free from errors. Furthermore, we shall not be liable in any event     
caused by the use of the program.                                     
 */

static char help[] = "3D TopOpt using KSP-MG on PETSc's DMDA (structured grids) \n";

int main(int argc, char *argv[]){

	// Error code for debugging
	PetscErrorCode ierr;

	/* new variables */
	//PetscInt       optimizer;
	std::string    workdir = "./";
	char           workdirChar[PETSC_MAX_PATH_LEN];
	std::string    filenameDiag = "./";
	FILE           *diag_fp = NULL;
	PetscBool      flg;
	typedef enum : PetscInt {MMA_t=0, OC_t=1} optimizer_type;
	optimizer_type  optimizer;

	// Initialize PETSc / MPI and pass input arguments to PETSc
	PetscInitialize(&argc,&argv,PETSC_NULL,help);

	/* Set workdir */
	PetscOptionsGetString(NULL,"-workdir",workdirChar,sizeof(workdirChar),&flg);
	if (flg){
		workdir = "";
		workdir.append(workdirChar);
	}

	/* Open file "diagnostics.dat" in workdir and write header */
	filenameDiag = workdir + "/diagnostics.dat";
	PetscPrintf(PETSC_COMM_WORLD,"%s\n",filenameDiag.c_str());
	diag_fp = fopen(filenameDiag.c_str(),"w");
	if (!diag_fp) SETERRQ1(PETSC_COMM_SELF,PETSC_ERR_SYS,"Cannot open file = %s \n",
						   filenameDiag.c_str());
	PetscFPrintf(PETSC_COMM_WORLD,diag_fp,"It.\tobj\t.g[0]\tch.\ttime\n");

	// STEP 1: THE OPTIMIZATION PARAMETERS, DATA AND MESH (!!! THE DMDA !!!)
	TopOpt *opt = new TopOpt();

	// STEP 2: THE PHYSICS
	LinearElasticity *physics = new LinearElasticity(opt);

	// STEP 3: THE FILTERING
	Filter *filter = new Filter(opt);
	
	// STEP 4: VISUALIZATION USING VTK
	MPIIO *output = new MPIIO(opt->da_nodes,3,"ux, uy, uz",2,"x, xPhys");

	/* Get option - cast optimzers address to PetscInt */
	PetscOptionsGetInt(NULL,"-optimizer",(PetscInt*)&optimizer,&flg);
	if (!flg)
		optimizer=MMA_t; /* Use MMA if not specified */
	// STEP 5: THE OPTIMIZER
	PetscInt itr=0;
	MMA *mma = NULL;
	OC *oc = NULL;

	PetscPrintf(PETSC_COMM_WORLD,
				"##############################################################\n");
	if (optimizer == MMA_t){/* MMA */
		PetscPrintf(PETSC_COMM_WORLD,"# Using MMA optimizer\n");
		opt->AllocateMMAwithRestart(&itr, &mma); // allow for restart !
	} else if ( optimizer == OC_t){ /* OC */
		PetscPrintf(PETSC_COMM_WORLD,"# Using OC optimizer\n");
		oc = new OC(opt->m);
	} else { /* Unknown */
		PetscPrintf(PETSC_COMM_WORLD,"# Unknown optimizer, aborting... \n");
		ierr = -1; CHKERRQ(ierr);
	}

	// STEP 6: FILTER THE INITIAL DESIGN/RESTARTED DESIGN
	ierr = filter->FilterProject(opt); CHKERRQ(ierr);
	
	// STEP 7: OPTIMIZATION LOOP
	PetscScalar ch = 1.0;
	double t1,t2;
	while (itr < opt->maxItr && ch > 0.01){
		// Update iteration counter
		itr++;

		// start timer
		t1 = MPI_Wtime();

		// Compute (a) obj+const, (b) sens, (c) obj+const+sens 
		ierr = physics->ComputeObjectiveConstraintsSensitivities(opt); CHKERRQ(ierr);
		
		// Compute objective scale
		if (itr==1){ 
			opt->fscale = 10.0/opt->fx; 
		}
		// Scale objectie and sens
		opt->fx = opt->fx*opt->fscale;
		VecScale(opt->dfdx,opt->fscale);

		// Filter sensitivities (chainrule)
		ierr = filter->Gradients(opt); CHKERRQ(ierr);

		// Sets outer movelimits on design variables
		if (optimizer == MMA_t){/* MMA */
			ierr = mma->SetOuterMovelimit(opt->Xmin,opt->Xmax,opt->movlim,opt->x,
										  opt->xmin,opt->xmax); CHKERRQ(ierr);
		} else if ( optimizer == OC_t){ /* OC */
			ierr = oc->SetOuterMovelimit(opt->Xmin,opt->Xmax,opt->movlim,opt->x,
										 opt->xmin,opt->xmax); CHKERRQ(ierr);
		}

		// Update design
		if (optimizer == MMA_t){/* MMA */
			ierr = mma->Update(opt->x,opt->dfdx,opt->gx,opt->dgdx,opt->xmin,opt->xmax); CHKERRQ(ierr);
		} else if ( optimizer == OC_t){ /* OC */
			ierr = oc->Update(opt->x,opt->dfdx,opt->gx,opt->dgdx,opt->xmin,opt->xmax,opt->volfrac); CHKERRQ(ierr);
		}

		// Inf norm on the design change
		if (optimizer == MMA_t){/* MMA */
			ch = mma->DesignChange(opt->x,opt->xold);
		} else if ( optimizer == OC_t){ /* OC */
			ch = oc->DesignChange(opt->x,opt->xold);
		}

		// Filter design field
		ierr = filter->FilterProject(opt); CHKERRQ(ierr);

		// stop timer
		t2 = MPI_Wtime();

		// Print to screen
		PetscPrintf(PETSC_COMM_WORLD,"It.: %i, obj.: %f, g[0]: %f, ch.: %f, time: %f\n",
				itr,opt->fx,opt->gx[0], ch,t2-t1);

		/* Print to diagnostics.dat (rank0 only) */
		PetscFPrintf(PETSC_COMM_WORLD,diag_fp,"%i\t%f\t%f\t%f\t%f\n",
					 itr,opt->fx,opt->gx[0], ch,t2-t1);

		// Write field data: first 10 iterations and then every 20th
		if (itr<11 || itr%20==0){
			output->WriteVTK(opt->da_nodes,physics->GetStateField(),opt, itr);
		}

		// Dump data needed for restarting code at termination
		if (itr%3==0 && optimizer == MMA_t){
			opt->WriteRestartFiles(&itr, mma);
			physics->WriteRestartFiles();
		}
	}
	// Write restart WriteRestartFiles
	if (optimizer == MMA_t){
		opt->WriteRestartFiles(&itr, mma);
		physics->WriteRestartFiles();
	}

	// Dump final design
	output->WriteVTK(opt->da_nodes,physics->GetStateField(),opt, itr+1);

	// STEP 7: CLEAN UP AFTER YOURSELF
	if (mma!=NULL){ delete mma;}
	if (oc!=NULL){ delete oc;}
	delete output;
	delete filter;
	delete opt;
	delete physics;

	// Finalize PETSc / MPI
	PetscFinalize();
	return 0;
}
