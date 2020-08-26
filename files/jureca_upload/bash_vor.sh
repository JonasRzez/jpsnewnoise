#!/bin/bash -x
#SBATCH -J db
#SBATCH --account=jias70
#SBATCH --nodes=1
#SBATCH --ntasks-per-node=2
#SBATCH --cpus-per-task=12
#SBATCH --output=out.%j
#SBATCH --error=err.%j
#SBATCH --time=20:00:00
#SBATCH --mail-user=j.rzezonka@fz-juelich.de
#SBATCH --mail-type=ALL

module load Python
module load SciPy-Stack/2019a-Python-3.6.8

export OMP_NUM_THREADS=${SLURM_CPUS_PER_TASK}

python ini_vor.py
wait
srun sh -c 'python vor_$(($SLURM_PROCID+1)).py'
wait
python errorplot.sh

rm vor_*
