// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2023 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Tests/Test.h>

class SoftBodyFrictionTest : public Test
{
public:
	JPH_DECLARE_RTTI_VIRTUAL(JPH_NO_EXPORT, SoftBodyFrictionTest)

	// Description of the test
	virtual const char *GetDescription() const override
	{
		return "Tests soft bodies with various values for friction. Note that this has very little effect.";
	}

	// See: Test
	virtual void		Initialize() override;
};
