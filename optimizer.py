#!/usr/bin/python2
"""
Kald med
python optimizer.py jobtype1 jobtype2 osv

jobtype er givet herunder.
"""

from subprocess import Popen, PIPE
import shlex
import time as t
import numpy as np
import os
from shutil import copy
import time
import sys

def set_jobtype(jobtype):
    if jobtype == "filter":
        workdir_base = "filter"
        # filter types (0=sens., 1=dens, 2=PDE, 3=None)
        opts = [0,1,2]
    elif jobtype == "penal":
        workdir_base = "penal"
        opts = [1,3,5]
    elif jobtype == "volfrac":
        workdir_base = "volfrac"
        opts = [0.2,0.4,0.6]
    else:
        print "jobtype not recognized."

    return workdir_base, opts


# template jobscript
jobscript_base = """
#!/bin/sh

### run with
### qsub jobscript.sh
### http://beige.ucs.indiana.edu/I590/node35.html
### qstat: R: running, C:completed, Q: queued, S: suspended,
###        H:held this means that it is not going to run until it is released
### showq:alle jobs

### embedded options to qsub - start with #PBS
### -- our name ---
#PBS -N topopt_{name}_{arg}
#PBS -l nodes=1:ppn=20
#PBS -l walltime=24:00:00

#PBS -o {workdir}/stdout/$PBS_JOBNAME.$PBS_JOBID.out
#PBS -e {workdir}/stdout/$PBS_JOBNAME.$PBS_JOBID.err
### -- run in the current working (submission) directory --
cd $PBS_O_WORKDIR

module load petsc/3.6.3-march-2016

#MPIRUN=/appl/petsc/3.6.3-march-2016/XeonX5550/bin/mpirun
MPIRUN=mpirun
# ==== EXECUTED COMANDS ====
$MPIRUN -np 20 -mca btl openib,self ./topopt -restart 0 -workdir {workdir} -{arg_type} {arg} -loadcase 2 > {workdir}/output.out
"""

def submit_jobs(jobtype):
    
    workdir_base, opts = set_jobtype(jobtype)
    # opts = [0]
    for i, opt in enumerate(opts):
        workdir = "{0}/{1}".format(workdir_base, opt)
        if not os.path.exists(workdir):
            os.makedirs(workdir)
        stdout = "%s/stdout" %(workdir)
        if not os.path.exists(stdout):
            os.makedirs(stdout)
        copy("petscrc",workdir)
        copy("bin2vtu.py",workdir)
        copy("makevtu.py",workdir)

        # write jobscript to file
        jobscript = jobscript_base.format(workdir=workdir,arg_type=workdir_base,arg=opt,name=workdir_base)
        with open("jobscript.sh", "w") as jobscript_file:
            jobscript_file.write(jobscript)
        copy("jobscript.sh",workdir)

        cmd = "qsub jobscript.sh"
        #process = Popen(shlex.split(cmd))
        # send stdout of qsub to hell(/dev/null)
        with open(os.devnull, 'w') as devnull:
            process = Popen(shlex.split(cmd),stdout=devnull)

        print "job, {0}: {1} submitted ".format(workdir_base,opt)

        # sleep 1 sec, wait for qsub to complete before the jobscript is overwritten
        time.sleep(1)


# Make sure main is only called when the file is executed
if __name__ == "__main__":

    args = sys.argv[1:]
    for jobtype in args:
        submit_jobs(jobtype)
        
    if not args:
        print "Script must be called as '%s jobtype'"%(sys.argv[0])
        sys.exit()
