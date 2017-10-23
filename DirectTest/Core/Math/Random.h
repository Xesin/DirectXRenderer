//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author:  James Stanard 
//

#pragma once

#include "Common.h"
#include <random>

namespace Math
{
    class RandomNumberGenerator
    {
    public:
        RandomNumberGenerator() : generator(m_rd())
        {
        }

        // Default int range is [MIN_INT, MAX_INT].  Max value is included.
        int32_t NextInt( void )
        {
            return std::uniform_int_distribution<int32_t>(0x80000000, 0x7FFFFFFF)(generator);
        }

        int32_t NextInt( int32_t MaxVal )
        {
            return std::uniform_int_distribution<int32_t>(0, MaxVal)(generator);
        }

        int32_t NextInt( int32_t MinVal, int32_t MaxVal )
        {
            return std::uniform_int_distribution<int32_t>(MinVal, MaxVal)(generator);
        }

        // Default float range is [0.0f, 1.0f).  Max value is excluded.
        float NextFloat( float MaxVal = 1.0f )
        {
            return std::uniform_real_distribution<float>(0.0f, MaxVal)(generator);
        }

        float NextFloat( float MinVal, float MaxVal )
        {
            return std::uniform_real_distribution<float>(MinVal, MaxVal)(generator);
        }

        void SetSeed( UINT s )
        {
            generator.seed(s);
        }

    private:

        std::random_device randomDevice;
        std::minstd_rand generator;
    };

    extern RandomNumberGenerator randomGenerator;
};
