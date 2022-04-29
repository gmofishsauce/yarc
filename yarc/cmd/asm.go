/*
Copyright © 2022 NAME HERE <EMAIL ADDRESS>

*/
package cmd

import (
	"github.com/spf13/cobra"

	"yarc/asm"
)

// asmCmd represents the asm command
var asmCmd = &cobra.Command{
	Use:   "asm sourceFile",
	Short: "The YARC microcode assembler / assembler / ALU specifier",
	Long: `Asm is the language processor for the language that defines the
YARC instruction set, FORTH language bootstrap, and ALU behaviors.

The language accepted by the processor is defined in the doc/ directory
found in the repo containing the source code for this command. More
information about the requirements and design considerations for the
processor are found in the package comment for yarc/asm, also in this
repo. The processor requires that exactly one source file conforming
to the language be provided on the command line. Within that file,
.include directives may be used to pull together multiple source files
used to create the binary download file for YARC.
`,

	Args: cobra.ExactArgs(1),
	Run: func(cmd *cobra.Command, args []string) {
		asm.Assemble(args[0])
	},
}

func init() {
	rootCmd.AddCommand(asmCmd)
}
