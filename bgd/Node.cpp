#include "Node.h"
#include <random>
#include <math.h>
#include <iostream>
#include <time.h>
#include <stdlib.h>
#include <chrono>
#include <algorithm>

using namespace std;

Node::Node(int dim, int nodeID, int layerID, int maxStaleInputs, NodeType type=NodeType::ReLU)
{
    _dim = dim;
    _IDinLayer = nodeID;
    _type = type;
    _layerNum = layerID;

    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::default_random_engine generator(seed);
    std::normal_distribution<float> distribution(0.0, 0.01);

    _weights = new float[_dim]();
    _mirrorWeights = new float[_dim]();

    if (ADAM)
    {
        _adamAvgMom = new float[_dim]();
        _adamAvgVel = new float[_dim]();
        _t = new float[_dim]();
    }
    _lastActivations = new float[BATCHSIZE]();
    _lastDeltaforBPs = new float[BATCHSIZE]();
    _ActiveinputIds = new int[BATCHSIZE]();

    for (size_t i = 0; i < _dim; i++)
    {
        _weights[i] = distribution(generator);
        _mirrorWeights[i] = _weights[i];
    }
    _bias = 0;
    _mirrorbias = _bias;
}

float Node::getLastActivation(int inputID)
{
    if(_ActiveinputIds[inputID] != 1)
        return 0.0;
    return _lastActivations[inputID];
}

void Node::incrementDelta(int inputID, float incrementValue)
{
    assert(("Input Not Active but still called !! BUG", _ActiveinputIds[inputID] == 1));
    if (_lastActivations[inputID]>0)
        _lastDeltaforBPs[inputID] += incrementValue;
}

float Node::getActivation(int* indices, float* values, int length, int inputID)
{
    assert(("Input ID more than Batch Size", inputID <= BATCHSIZE));

    //FUTURE TODO: shrink batchsize and check if input is alread active then ignore and ensure backpopagation is ignored too.
    _ActiveinputIds[inputID] = 1; //activate input
    _lastActivations[inputID] = 0;
    for (int i = 0; i < length; i++)
    {
        _lastActivations[inputID] += _weights[indices[i]] * values[i];
    }
    _lastActivations[inputID] += _bias;

//    switch (_type)
//    {
//        case NodeType::ReLU:
//            if (_lastActivations[inputID] < 0)
//                _lastActivations[inputID] = 0;
//            _lastDeltaforBPs[inputID] = 0;
//            break;
//        case NodeType::Softmax:
//            break;
//        default:
//            cout << "Invalid Node type from Constructor" <<endl;
//            break;
//    }

    if (_type==NodeType::ReLU){
        if (_lastActivations[inputID] < 0)
            _lastActivations[inputID] = 0;
        _lastDeltaforBPs[inputID] = 0;
    }
//    _lastDeltaforBPs[inputID] = 0;

    return _lastActivations[inputID];
}

void Node::ComputeExtaStatsForSoftMax(float normalizationConstant, int inputID, int label)
{
    assert(("Input Not Active but still called !! BUG", _ActiveinputIds[inputID] ==1));

    _lastActivations[inputID] /= normalizationConstant + 0.0000001;

    //TODO:check  gradient
    if (_IDinLayer == label)
        _lastDeltaforBPs[inputID] = (1 - _lastActivations[inputID])/BATCHSIZE;
    else
        _lastDeltaforBPs[inputID] = (-_lastActivations[inputID])/BATCHSIZE;


}

void Node::backPropagate(Node** previousNodes, int* previousLayerActiveNodeIds, int previousLayerActiveNodeSize, float learningRate, int inputID)
{
    assert(("Input Not Active but still called !! BUG", _ActiveinputIds[inputID] == 1));

    for (int i = 0; i < previousLayerActiveNodeSize; i++)
    {
        //UpdateDelta before updating weights
        previousNodes[previousLayerActiveNodeIds[i]]->incrementDelta(inputID, _lastDeltaforBPs[inputID] * _weights[previousLayerActiveNodeIds[i]]);
        float grad_t = _lastDeltaforBPs[inputID] * previousNodes[previousLayerActiveNodeIds[i]]->getLastActivation(inputID);
        float grad_tsq = grad_t * grad_t;
        if (ADAM)
        {
            _t[previousLayerActiveNodeIds[i]] += grad_t;
//            _adamAvgMom[previousLayerActiveNodeIds[i]] = BETA1 * _adamAvgMom[previousLayerActiveNodeIds[i]] + (1 - BETA1)*grad_t;
//            _adamAvgVel[previousLayerActiveNodeIds[i]] = BETA2 * _adamAvgVel[previousLayerActiveNodeIds[i]] + (1 - BETA2)*grad_tsq;

//            _mirrorWeights[previousLayerActiveNodeIds[i]] +=learningRate* (1 / (sqrt(_adamAvgVel[previousLayerActiveNodeIds[i]]) + EPS)) * _adamAvgMom[previousLayerActiveNodeIds[i]];

        }
        else
        {
            _mirrorWeights[previousLayerActiveNodeIds[i]] += learningRate * grad_t;
        }
    }

    if (ADAM)
    {
        float biasgrad_t = _lastDeltaforBPs[inputID];
        float biasgrad_tsq = biasgrad_t * biasgrad_t;
        _tbias += biasgrad_t;

//		_adamAvgMombias = _adamAvgMombias / (1 - pow(BETA1, _tbias));
//		_adamAvgVelbias = _adamAvgVelbias / (1 - pow(BETA2, _tbias));


//        _mirrorbias += learningRate*(1 / (sqrt(_adamAvgVelbias) + EPS)) * _adamAvgMombias;

    }
    else{
        _mirrorbias += learningRate * _lastDeltaforBPs[inputID];
    }

    _ActiveinputIds[inputID] = 0;
    _lastDeltaforBPs[inputID] = 0;
}

void Node::backPropagateFirstLayer(int* nnzindices, float* nnzvalues, int nnzSize, float learningRate, int inputID)
{
    assert(("Input Not Active but still called !! BUG", _ActiveinputIds[inputID] == 1));

    for (int i = 0; i < nnzSize; i++)
    {
        float grad_t = _lastDeltaforBPs[inputID] * nnzvalues[i];
        float grad_tsq = grad_t * grad_t;
        if (ADAM)
        {
            _t[nnzindices[i]] += grad_t;
        }
        else {
            _mirrorWeights[nnzindices[i]] += learningRate * grad_t;
        }
    }

    if (ADAM)
    {
        float biasgrad_t = _lastDeltaforBPs[inputID];
        float biasgrad_tsq = biasgrad_t * biasgrad_t;
        _tbias += biasgrad_t;

    }else {
        _mirrorbias += learningRate * _lastDeltaforBPs[inputID];
    }

    _ActiveinputIds[inputID] = 0;//deactivate inputIDs
    _lastDeltaforBPs[inputID] = 0;
}


Node::~Node()
{

    delete[] _indicesInTables;
    delete[] _indicesInBuckets;
    delete[] _lastActivations;
    delete[] _lastDeltaforBPs;
    delete[] _ActiveinputIds;
}

// for debugging gradients.
float Node::purturbWeight(int weightid, float delta)
{
    _weights[weightid] += delta;
    return _weights[weightid];
}

float Node::getGradient(int weightid, int inputID, float InputVal)
{
    return -_lastDeltaforBPs[inputID] * InputVal;
}

float* Node::getTestActivation() {
    return _lastActivations;
}

float* Node::getLastDeltaForBPs() {
    return _lastDeltaforBPs;

}