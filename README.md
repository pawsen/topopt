TopOpt_in_PETSc
===============
A 3D large-scale topology optimization code using PETSc
===============

The code (or framework) presented on this page is a fully parallel framework for conducting very large scale topology optimziation on structured grids. For more
details see www.topopt.dtu.dk/PETSc.

To clone repository:
>> git clone https://github.com/topopt/TopOpt_in_PETSc.git

NOTE: The code requires PETSc version 3.6.0 or newer !

Create statefile for paraview:

Create as:
* Open your TopOpt result in ParaView and press apply.
* Calculator: Create a magnitude field of the displacements by typing
  the following in expression box ux\*iHat+uy\*jHat+uz\*kHat. By default
  the field is named ’Result’, rename it to disp.
* Clean-to-grid: Removes double nodes
* Cell-data-to-point-data: As the name says
* Iso-Volume: Use xPhys as input scalar. Set the threshold to min = 0.5
  and max = 1 and press apply.
* Extract-Surface: As the name says
* Triangulate: As the name says
* Decimate: reduce the number of surface triangles. IMPORTANT: check the
  'Preserve topology' box.
* Save as STL under File.

You can now 3D print the result on your favorite printer. Note that for
pure visu- alization purposes you may (and should) stop after creating
the IsoVolume.
