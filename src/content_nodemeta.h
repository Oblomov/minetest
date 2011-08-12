/*
Minetest-c55
Copyright (C) 2010-2011 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef CONTENT_NODEMETA_HEADER
#define CONTENT_NODEMETA_HEADER

#include "nodemetadata.h"

class Inventory;

class SignNodeMetadata : public NodeMetadata
{
public:
	SignNodeMetadata(const std::string &text);
	//~SignNodeMetadata();
	
	virtual u16 typeId() const;
	static NodeMetadata* create(std::istream &is);
	virtual NodeMetadata* clone() const;
	virtual void serializeBody(std::ostream &os) const;
	virtual std::string infoText() const;

	std::string getText() const { return m_text; }
	void setText(std::string t){ m_text = t; }

private:
	std::string m_text;
};

class ChestNodeMetadata : public NodeMetadata
{
public:
	ChestNodeMetadata();
	~ChestNodeMetadata();
	
	virtual u16 typeId() const;
	static NodeMetadata* create(std::istream &is);
	virtual NodeMetadata* clone() const;
	virtual void serializeBody(std::ostream &os) const;
	virtual std::string infoText() const;
	virtual const Inventory* getInventory() const {return m_inventory;}
	virtual Inventory* getInventory() {return m_inventory;}
	virtual bool nodeRemovalDisabled() const;
	virtual std::string getInventoryDrawSpecString() const;
	
private:
	Inventory *m_inventory;
};

class FurnaceNodeMetadata : public NodeMetadata
{
public:
	FurnaceNodeMetadata();
	~FurnaceNodeMetadata();
	
	virtual u16 typeId() const;
	virtual NodeMetadata* clone() const;
	static NodeMetadata* create(std::istream &is);
	virtual void serializeBody(std::ostream &os) const;
	virtual std::string infoText() const;
	virtual const Inventory* getInventory() const {return m_inventory;}
	virtual Inventory* getInventory() {return m_inventory;}
	virtual void inventoryModified() const;
	virtual bool step(float dtime);
	virtual std::string getInventoryDrawSpecString() const;

private:
	Inventory *m_inventory;
	float m_step_accumulator;
	float m_fuel_totaltime;
	float m_fuel_time;
	float m_src_totaltime;
	float m_src_time;
};


#endif

