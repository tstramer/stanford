/*********
 CONSTANTS
 *********/

#define GRAVITY_ACCEL (-9.8f)

#define MAX_PARTICLES_PER_BIN 3199
#define MAX_NEIGHBORS 128

#ifndef M_PI
    #define M_PI (3.14159265358979323846f)
#endif

#define POLY6_CONSTANT  (315.0f / (64.0f * M_PI))
#define SPIKY_GRADIENT_CONSTANT (-45.0f / M_PI)

/**
 * Ignore particles with a distance apart less than this value
 * to avoid values blowing up or divide-by-zero errors
 */
#define EPSILON (.0000005f)


/****************
 TYPE DEFINITIONS
 ****************/

typedef struct{
	float4 vel;
    float4 pos;
    float4 predPos;
    unsigned int bin[4]; // last int is the index
    int neighbors[MAX_NEIGHBORS];
    float lambda;
    float dummy[3]; // needed for alignment
    float4 dx;
    float4 vorticity;
} Particle;

typedef struct {
    float timeStep;
    float particleRadius;
    float kernelRadius;
    int numParticles;
    
    int numBinsPerDim;
    int numSolverIterations;
    float restDensity;
    float cfmParam;
    
    float artificialPressureStrength;
    float artificialPressureRadius;
    float artificialPressurePower;
    float artificialViscosity;
    
    float vorticityParam;
    float dummy[3];
} Parameters;

typedef struct BinParticles {
    int particles[MAX_PARTICLES_PER_BIN];
    int count;
} BinParticles;


/***************************
 HELPER FUNCTION DEFINITIONS
 ***************************/

void clampPosition(__global Particle* p, __global Parameters* params, const float3 bounds);
unsigned int calculateBin(__global Particle* p, int dim, const float3 bounds, int numBinsPerDim);
unsigned int binToIdx(int x, int y, int z, int numBinsPerDim);
float calculateGradientSumSqrd(__global Particle* particles, int i, __global Parameters* params);
float sphDensityEstimatorSpikyGradient_ieqk(__global Particle* particles, int i, __global Parameters* params);
float sphDensityEstimatorSpikyGradient_ineqk(__global Particle* particles, int i, __global Parameters* params);
float sphDensityEstimatorPoly6(__global Particle* particles, int i, float h);
float poly6Kernel(float4 p, float h);
float4 spikyKernelGradient(float4 p, float h);
float calculateArtificialPressure(float4 p_i, float4 p_j, __global Parameters* params);
float4 calculateXSPHViscosity(__global Particle* particles, int i,  __global Parameters* params);
float4 calculateVorticityVelocity(__global Particle* particles, int i, __global Parameters* params);
float4 calculateVorticityGradient(__global Particle* particles, int i, __global Parameters* params);


/***************
 KERNEL FUNCTION
 ***************/

/**
 * Resets all intermediate variables used in each update loop.
 * Called once at the beginning of each update.
 */
__kernel void resetState(__global Particle* particles) {
    int id = get_global_id(0);
    global Particle *particle = &particles[id];
    particle->predPos = float4(0, 0, 0, 0);
    particle->dx = float4(0, 0, 0, 0);
    particle->lambda = 0;
    for (int i = 0; i < MAX_NEIGHBORS; i++) {
        particle->neighbors[i] = -1;
    }
}

/**
 * Called once per particle.
 * 
 * Corresponds to line 1-4 of the PBF algorithm.
 */
__kernel void predictPosition(__global Parameters* params, __global Particle* particles, const float3 bounds) {
    int id = get_global_id(0);
    global Particle *particle = &particles[id];
    
    particle->vel.y += GRAVITY_ACCEL * params->timeStep; // assumed unit mass
    particle->predPos = particle->pos + particle->vel * params->timeStep;
    
    clampPosition(particle, params, bounds);
}

/**
 * Called once per particle.
 *
 * Calculates the particle bin for a particle given its predicted location.
 * Stores the bin in an array of size 4 with elements:
 *   0: x component of bin
 *   1: y component of bin
 *   2: z component of bin
 *   3: index of bin in flattened bin array
 */
__kernel void calculateParticleBin(__global Parameters* params, __global Particle* particles, const float3 bounds) {
    int id = get_global_id(0);
    global Particle *particle = &particles[id];
    
    particle->bin[0] = calculateBin(particle, 0, bounds, params->numBinsPerDim);
    particle->bin[1] = calculateBin(particle, 1, bounds, params->numBinsPerDim);
    particle->bin[2] = calculateBin(particle, 2, bounds, params->numBinsPerDim);
    particle->bin[3] = binToIdx(particle->bin[0],  particle->bin[1],  particle->bin[2], params->numBinsPerDim);
}

/**
 * Called once per bin.
 *
 * Assigns particles to bins. This method is responsible for assigning particles to 
 * the specific bin it is handling. A 2d array is pre-allocated with size
 * (numbins x MAX_PARTICLES_PER_BIN) to store the mapping, which means it has a fixed upper
 * bound on the number of particles stored per bin. Further particles are ignored for
 * the sake of computational / memory savings.
 */
__kernel void assignParticlesToBins(__global Parameters* params, __global Particle* particles, __global BinParticles *binParticles) {
    unsigned int binIdx = get_global_id(0);
    global BinParticles *bp = &binParticles[binIdx];

    int count = 0;
    for (int i = 0; i < params->numParticles; i++) {
        if (binIdx == particles[i].bin[3]) {
            bp->particles[count] = i;
            count++;
        }
        if (count >= MAX_PARTICLES_PER_BIN) {
            break;
        }
    }
    
    bp->count = count;
}

/**
 * Called once per particle.
 *
 * Corresponds to lines 5-7 of PBF algo.
 *
 * This method handles finding neighbor particles of a given particle
 * by using the mapping from bin to particles and looking at particles in neighboring 
 * bins. The maximum number of neighbors stored is MAX_NEIGHBORS.
 */
__kernel void assignParticleNeighbors(__global Parameters* params, __global Particle* particles, __global BinParticles *binParticles) {
    int id = get_global_id(0);
    global Particle *particle = &particles[id];

    int binX = particle->bin[0];
    int binY = particle->bin[1];
    int binZ = particle->bin[2];

    int neighborCount = 0;
    
    for (int dx = -1; dx <= 1; dx++) {
        int binX1 = binX + dx;
        if (binX1 < 0 || binX1 >= params->numBinsPerDim) {
            continue;
        }
        if (neighborCount >= MAX_NEIGHBORS) {
            break;
        }

        for (int dy = -1; dy <= 1; dy++) {
            int binY1 = binY + dy;
            if (binY1 < 0 || binY1 >= params->numBinsPerDim) {
                continue;
            }
            if (neighborCount >= MAX_NEIGHBORS) {
                break;
            }

            for (int dz = -1; dz <= 1; dz++) {
                int binZ1 = binZ + dz;
                if (binZ1 < 0 || binZ1 >= params->numBinsPerDim) {
                    continue;
                }
                if (neighborCount >= MAX_NEIGHBORS) {
                    break;
                }

                unsigned int binIdx = binToIdx(binX1, binY1, binZ1, params->numBinsPerDim);
                global BinParticles *bp = &binParticles[binIdx];
                
                for (int otherIdx = 0; otherIdx < bp->count; otherIdx++) {
                    int otherID = bp->particles[otherIdx];
                    if (id == otherID) { continue; }
                    particle->neighbors[neighborCount] = otherID;
                    neighborCount++;
                    if (neighborCount >= MAX_NEIGHBORS) {
                        break;
                    }
                }
            }
        }
    }
}

/**
 * Called once per particle.
 *
 * Corresponds to lines 9-11 of PBF algo.
 */
__kernel void calculateLambdas(__global Parameters* params, __global Particle* particles) {
    int id = get_global_id(0);
    global Particle *particle = &particles[id];

    float p_i = sphDensityEstimatorPoly6(particles, id, params->kernelRadius);
    float c_i = (p_i / params->restDensity) - 1.0f;
    
    float gradSumSqrd = calculateGradientSumSqrd(particles, id, params);
    
    particle->lambda = -c_i / (gradSumSqrd + params->cfmParam);
}

/**
 * Called once per particle.
 *
 * Corresponds to lines 12-15 of PBF algo
 */
__kernel void calculatePositionChanges(__global Parameters* params, __global Particle* particles, const float3 bounds) {
    int id = get_global_id(0);
    global Particle *particle = &particles[id];

    float4 result = float4(0, 0, 0, 0);
    
    for (int j = 0; j < MAX_NEIGHBORS; j++) {
        int nbrIdx = particles->neighbors[j];
        if (nbrIdx == -1) {
            break;
        }
        global Particle *nbrParticle = &particles[nbrIdx];
        
        float4 p = spikyKernelGradient(particle->predPos - nbrParticle->predPos, params->kernelRadius);
        float sCorr = calculateArtificialPressure(particle->predPos, nbrParticle->predPos, params);
        p *= (particle->lambda + nbrParticle->lambda + sCorr);
        result += p;
    }

    particle->dx = result / params->restDensity;
}

/**
 * Called once per particle.
 *
 * Corresponds to lines 16-18 of PBF algo.
 */
__kernel void updatePredictedPositions(__global Parameters* params, __global Particle* particles, const float3 bounds) {
    int id = get_global_id(0);
    global Particle *particle = &particles[id];

    clampPosition(particle, params, bounds);
    particle->predPos += particle->dx;
}

/**
 * Called once per particle.
 *
 * Calculates the vorticity velocity for a given particle and stores it on the particle
 * struct. The actual application of vorticity is in updateVelocities.
 */
__kernel void calculateVorticity(__global Parameters* params, __global Particle* particles) {
    int id = get_global_id(0);
    global Particle *particle = &particles[id];
    float4 result = float4(0, 0, 0, 0);
    for (int j = 0; j < MAX_NEIGHBORS; j++) {
        int nbrIdx = particles->neighbors[j];
        if (nbrIdx == -1) {
            break;
        }
        global Particle *nbrParticle = &particles[nbrIdx];
        if (distance(particle->predPos, nbrParticle->predPos) > params->kernelRadius) {
            continue;
        }
        float4 g = spikyKernelGradient(particle->predPos - nbrParticle->predPos, params->kernelRadius);
        result += cross(particle->vel - nbrParticle->vel, g);
    }
    particle->vorticity = result;
}

/**
 * Called once per particle.
 *
 * Corresponds to lines 21-22 of PBF algo.
 */
__kernel void updateVelocities(__global Parameters* params, __global Particle* particles, __global float4* positions) {
    int id = get_global_id(0);
    global Particle *particle = &particles[id];
    particle->vel = (1.0f / params->timeStep) * (particle->predPos - particle->pos);
    particle->vel += calculateXSPHViscosity(particles, id, params);
    particle->vel += calculateVorticityVelocity(particles, id, params);
}

/**
 * Called once per particle.
 *
 * Corresponds to lines 23 of PBF algo.
 */
__kernel void updatePosition(__global Parameters* params, __global Particle* particles, __global float4* positions) {
    int id = get_global_id(0);
    global Particle *particle = &particles[id];
    particle->pos = particle->predPos;
    positions[id] = particle->pos;
    positions[id].w = 1;
}


/****************
 HELPER FUNCTIONS
 ****************/

/**
 * Clamps a particle's predicted position to be within the bounding box starting at the origin
 * and having width/depth/height defined by bounds
 */
void clampPosition(__global Particle* p, __global Parameters* params, const float3 bounds) {
    p->predPos.x = clamp(p->predPos.x, params->particleRadius, bounds.x - params->particleRadius);
    p->predPos.y = clamp(p->predPos.y, params->particleRadius, bounds.y - params->particleRadius);
    p->predPos.z = clamp(p->predPos.z, params->particleRadius, bounds.z - params->particleRadius);
}

/**
 * Converts a particle's predicted position into a bin index for a specific dimension (x, y, or z).
 */
unsigned int calculateBin(__global Particle* p, int dim, const float3 bounds, int numBinsPerDim) {
    float binSize = bounds[dim] / (float)numBinsPerDim;
    return clamp((int)floor(p->predPos[dim] / binSize), 0, numBinsPerDim - 1);
}

/**
 * Converts a 3D bin index into a flat array index
 */
unsigned int binToIdx(int x, int y, int z, int numBinsPerDim) {
    return x + y*numBinsPerDim + z*numBinsPerDim*numBinsPerDim;
}

float poly6Kernel(float4 p, float h) {
    float r = fast_length(p);
    if (r <= h && r >= EPSILON) {
        float pow_h_9 = h * h * h * h * h * h * h * h * h;
        if (pow_h_9 < EPSILON) { return 0.0f; }
        float main = h * h - r * r;
        return (POLY6_CONSTANT / pow_h_9) * main * main * main;
    } else {
        return 0;
    }
}

float4 spikyKernelGradient(float4 p, float h) {
    float r = fast_length(p);
    if (r <= h && r >= EPSILON) {
        float pow_h_6 = h * h * h * h * h * h;
        if (pow_h_6 < EPSILON) { return float4(0, 0, 0, 0); }
        float scale = (SPIKY_GRADIENT_CONSTANT / pow_h_6) * (h - r) * (h - r) / (r + EPSILON);
        return float4(p.x * scale, p.y * scale, p.z * scale, 0);
    } else {
        return float4(0, 0, 0, 0);
    }
}

/**
 * Calculates the SPH density estimator for a given particle and kernel radius
 * using the poly6 kernel.
 */
float sphDensityEstimatorPoly6(__global Particle* particles, int i, float h) {
    global Particle *particle = &particles[i];
    float result = 0;
    for (int j = 0; j < MAX_NEIGHBORS; j++) {
        int nbrIdx = particles->neighbors[j];
        if (nbrIdx == -1) {
            break;
        }
        global Particle *nbrParticle = &particles[nbrIdx];
        if (distance(particle->predPos, nbrParticle->predPos) > h) {
            continue;
        }
        result += poly6Kernel(particle->predPos - nbrParticle->predPos, h);
    }
    return result;
}

/**
 * Helper to calculate the gradient of the SPH density estimator for a given particle and
 * kernel radius using the spiky gradient kernel. This handles the case where i == k.
 */
float sphDensityEstimatorSpikyGradient_ieqk(__global Particle* particles, int i, __global Parameters* params) {
    global Particle *particle = &particles[i];
    float h = params->kernelRadius;
    
    float4 p = float4(0, 0, 0, 0);
    for (int j = 0; j < MAX_NEIGHBORS; j++) {
        int nbrIdx = particles->neighbors[j];
        if (nbrIdx == -1) {
            break;
        }
        global Particle *nbrParticle = &particles[nbrIdx];
        if (distance(particle->predPos, nbrParticle->predPos) > h) {
            continue;
        }
        p += spikyKernelGradient(particle->predPos - nbrParticle->predPos, h);
    }
    float l = length(p);
    return l * l;
}

/**
 * Helper to calculate the gradient of the SPH density estimator for a given particle and
 * kernel radius using the spiky gradient kernel. This handles the case where i != k.
 */
float sphDensityEstimatorSpikyGradient_ineqk(__global Particle* particles, int i, __global Parameters* params) {
    global Particle *particle = &particles[i];
    float h = params->kernelRadius;

    float result = 0;
    for (int j = 0; j < MAX_NEIGHBORS; j++) {
        int nbrIdx = particles->neighbors[j];
        if (nbrIdx == -1) {
            break;
        }
        global Particle *nbrParticle = &particles[nbrIdx];
        if (distance(particle->predPos, nbrParticle->predPos) > h) {
            continue;
        }
        float4 p = -spikyKernelGradient(particle->predPos - nbrParticle->predPos, h) / params->restDensity;
        float l = length(p);
        result += l * l;
    }
    return result;
}

/**
 * Calculates the denominator of equation 9 (calculateLambda),
 * which is sum of squares of the length of the gradient vectors.
 */
float calculateGradientSumSqrd(__global Particle* particles, int i, __global Parameters* params) {
    return sphDensityEstimatorSpikyGradient_ieqk(particles, i, params) + sphDensityEstimatorSpikyGradient_ineqk(particles, i, params);
}

/**
 * Calculates the artificial pressure for a given particle (sCorr), which is used
 * in calculatePositionChange when calculating the position change.
 */
float calculateArtificialPressure(float4 p_i, float4 p_j, __global Parameters* params) {
    float n = params->artificialPressurePower;
    float k = params->artificialPressureStrength;
    float q = params->kernelRadius * params->artificialPressureRadius;
    
    float4 dq = float4(q, q, q, 0) + p_i;
    
    float num = poly6Kernel(p_i - p_j, params->kernelRadius);
    float denom = poly6Kernel(dq, params->kernelRadius);
    
    if (denom <= EPSILON) {
        return 0;
    } else {
        return -k * pow(num / denom, n);
    }
}

/**
 * Equation 17 of PBF.
 */
float4 calculateXSPHViscosity(__global Particle* particles, int i,  __global Parameters* params) {
    global Particle *particle = &particles[i];
    float4 result = float4(0, 0, 0, 0);
    for (int j = 0; j < MAX_NEIGHBORS; j++) {
        int nbrIdx = particles->neighbors[j];
        if (nbrIdx == -1) {
            break;
        }
        global Particle *nbrParticle = &particles[nbrIdx];
        if (distance(particle->predPos, nbrParticle->predPos) > params->kernelRadius) {
            continue;
        }
        result += (particle->vel - nbrParticle->vel) * poly6Kernel(particle->predPos - nbrParticle->predPos, params->kernelRadius);
    }
    return (result * params->artificialViscosity);
}

/**
 * Equation 16 of PBF, as well as multiplying by dt to convert the force into
 * a velocity. Note that this assumes particles have unit mass (so F = A).
 */
float4 calculateVorticityVelocity(__global Particle* particles, int i, __global Parameters* params) {
    global Particle *particle = &particles[i];
    float4 result = calculateVorticityGradient(particles, i, params);
    float l = length(result);
    if (l > 0) {
        result = normalize(result);
    }
    return cross(result, particle->vorticity) * params->timeStep * params->vorticityParam;
}

/**
 * Calculates the gradient of vorticity for a particle.
 */
float4 calculateVorticityGradient(__global Particle* particles, int i, __global Parameters* params) {
    global Particle *particle = &particles[i];
    float4 result = float4(0, 0, 0 ,0);
    for (int j = 0; j < MAX_NEIGHBORS; j++) {
        int nbrIdx = particles->neighbors[j];
        if (nbrIdx == -1) {
            break;
        }
        global Particle *nbrParticle = &particles[nbrIdx];
        if (distance(particle->predPos, nbrParticle->predPos) > params->kernelRadius) {
            continue;
        }
        float4 p_ij = particle->predPos - nbrParticle->predPos;
        float len = length(particle->vorticity - nbrParticle->vorticity);
        if (p_ij.x != 0 && p_ij.y != 0 && p_ij.z != 0) {
            result += float4(len / p_ij.x, len / p_ij.y, len / p_ij.z, 0);
        }
    }
    return result;
}
