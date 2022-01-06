//##########################################################################
//#                                                                        #
//#                               CCLIB                                    #
//#                                                                        #
//#  This program is free software; you can redistribute it and/or modify  #
//#  it under the terms of the GNU Library General Public License as       #
//#  published by the Free Software Foundation; version 2 or later of the  #
//#  License.                                                              #
//#                                                                        #
//#  This program is distributed in the hope that it will be useful,       #
//#  but WITHOUT ANY WARRANTY; without even the implied warranty of        #
//#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the          #
//#  GNU General Public License for more details.                          #
//#                                                                        #
//#          COPYRIGHT: EDF R&D / TELECOM ParisTech (ENST-TSI)             #
//#                                                                        #
//##########################################################################

#include <ScalarField.h>

//System
#include <cassert>
#include <cstring>


using namespace CCLib;

ScalarField::ScalarField(const char* name/*=0*/)
{
	setName(name);
}

ScalarField::ScalarField(const ScalarField& sf)
	: std::vector<ScalarType>(sf)
{
	setName(sf.m_name);
}

void ScalarField::setName(const char* name)
{
	if (name)
		strncpy(m_name, name, 255);
	else
		strcpy(m_name, "Undefined");
}

void ScalarField::saveSFBinary(char* fileName) {
    std::ofstream ofs(fileName, std::ios::trunc | std::ios::binary);
    if (ofs.is_open())
    {
        //write struct to binary file then close binary file
        ofs.write((char*)data(), size()*sizeof(ScalarType));
        ofs.close();
    }
}

void ScalarField::saveSFTxt(char* fileName) {
    std::ofstream output_file(fileName);
	for (auto it = begin(); it != end(); ++it) {
		output_file << *it << ",";
	}
}

void ScalarField::computeMeanAndVariance(ScalarType &mean, ScalarType* variance) const
{
	double _mean = 0.0;
	double _std2 = 0.0;
	std::size_t count = 0;

	for (std::size_t i = 0; i < size(); ++i)
	{
		const ScalarType& val = at(i);
        if (ValidValue(val))
        {
            _mean += val;
            _std2 += static_cast<double>(val) * val;
            ++count;
        }
    }

    if (count)
    {
        _mean /= count;
        mean = static_cast<ScalarType>(_mean);

        if (variance)
        {
            _std2 = std::abs(_std2 / count - _mean * _mean);
            *variance = static_cast<ScalarType>(_std2);
        }
    }
    else
    {
        mean = 0;
        if (variance)
        {
            *variance = 0;
        }
    }
}

void ScalarField::compute3stdProbability(std::array<float, 3>& probability, ScalarType& mean, ScalarType& stdv) {
    double count_std1 = 0;
    double count_std2 = 0;
    double count_std3 = 0;
    double _val;
    double _mean = static_cast<double>(mean);
	double _stdv = static_cast<double>(stdv);
    for (std::size_t i = 0; i < size(); ++i)
    {
        const ScalarType& val = at(i);
        if (ValidValue(val))
        {
			_val = static_cast<double>(val);
			if(_val >= _mean - 3* _stdv && _val <= _mean + 3 * _stdv)
			{
                ++count_std3;
			}
			if (_val >= _mean - 2 * _stdv && _val <= _mean + 2 * _stdv) {
                ++count_std2;
			}
			if (_val >= _mean - _stdv && _val <= _mean + _stdv) {
				++count_std1;
			}
        }
    }
	probability[0] = count_std1 / size();
	probability[1] = count_std2 / size();
	probability[2] = count_std3 / size();
}

bool ScalarField::reserveSafe(std::size_t count)
{
	try
	{
		reserve(count);
	}
	catch (const std::bad_alloc&)
	{
		//not enough memory
		return false;
	}
	return true;
}

bool ScalarField::resizeSafe(std::size_t count, bool initNewElements/*=false*/, ScalarType valueForNewElements/*=0*/)
{
	try
	{
		if (initNewElements)
			resize(count, valueForNewElements);
		else
			resize(count);
	}
	catch (const std::bad_alloc&)
	{
		//not enough memory
		return false;
	}
	return true;
}
