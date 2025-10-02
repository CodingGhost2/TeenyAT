case TNY_OPCODE_PSH:
			t->reg[TNY_REG_SP].u &= TNY_MAX_RAM_ADDRESS;
			if(TNY_REG_ZERO){
				t->ram[t->reg[0].u].u = t->reg[reg2].s + immed;
			}
			else if(TNY_REG_PC){
				t->ram[t->reg[1].u].u = t->reg[reg2].s + immed;
			}
			else if(TNY_REG_SP){
				t->ram[t->reg[2].u].u = t->reg[reg2].s + immed;
			}
			else if(TNY_REG_A){
				t->ram[t->reg[3].u].u = t->reg[reg2].s + immed;
			}
			else if(TNY_REG_B){
				t->ram[t->reg[4].u].u = t->reg[reg2].s + immed;
			}
			else if(TNY_REG_C){
				t->ram[t->reg[5].u].u = t->reg[reg2].s + immed;
			}
			else if(TNY_REG_D){
				t->ram[t->reg[6].u].u = t->reg[reg2].s + immed;
			}
			else if(TNY_REG_E){
				t->ram[t->reg[7].u].u = t->reg[reg2].s + immed;
			}
			t->reg[TNY_REG_SP].u--;
			t->reg[TNY_REG_SP].u &= TNY_MAX_RAM_ADDRESS;
			/*
			* To promote student use of registers, all bus operations,
			* including RAM access comes with an extra penalty.
			*/
			t->delay_cycles += TNY_BUS_DELAY;
			break;
case TNY_OPCODE_POP:
			t->reg[TNY_REG_SP].u++;
			t->reg[TNY_REG_SP].u &= TNY_MAX_RAM_ADDRESS;
            if(TNY_REG_ZERO){
				t->ram[t->reg[0].u].u = t->reg[reg1].s + immed;
			}
			else if(TNY_REG_PC){
				t->ram[t->reg[1].u].u = t->reg[reg1].s + immed;
			}
			else if(TNY_REG_SP){
				t->ram[t->reg[2].u].u = t->reg[reg1].s + immed;
			}
			else if(TNY_REG_A){
				t->ram[t->reg[3].u].u = t->reg[reg1].s + immed;
			}
			else if(TNY_REG_B){
				t->ram[t->reg[4].u].u = t->reg[reg1].s + immed;
			}
			else if(TNY_REG_C){
				t->ram[t->reg[5].u].u = t->reg[reg1].s + immed;
			}
			else if(TNY_REG_D){
				t->ram[t->reg[6].u].u = t->reg[reg1].s + immed;
			}
			else if(TNY_REG_E){
				t->ram[t->reg[7].u].u = t->reg[reg1].s + immed;
			}
			/*
			* To promote student use of registers, all bus operations,
			* including RAM access comes with an extra penalty.
			*/
			t->delay_cycles += TNY_BUS_DELAY;
			break;