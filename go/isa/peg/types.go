package peg

import "fmt"
import "strings"

/* --- Types --- */ 

type node interface {
	Kind() string
	Value() []byte
	String() string
	IndentString(indent int) string
	Kids() []node
}

type ParentNode struct {
	kind string
	kids []node
}

type TerminalNode struct {
	kind string
	val []byte
}

/* --- Factory methods --- */

func NewParentNode(kind string, kids []node) *ParentNode {
	return &ParentNode{kind, kids}
}

func NewTerminalNode(kind string, val []byte) *TerminalNode {
	return &TerminalNode{kind, val}
}

/* --- Methods of ParentNode --- */

func (p *ParentNode) Kind() string {
	return p.kind
}

func (p *ParentNode) Value() []byte {
	return []byte{}
}

func (p *ParentNode) Kids() []node {
	return p.kids
}

func (p *ParentNode) String() string {
	return p.IndentString(0)
}

// Print self, with 2*indent leading spaces and no newline.
func (p *ParentNode) IndentString(indent int) string {
	var b strings.Builder
	indentStr := strings.Repeat("  ", indent)

	b.WriteString(indentStr)
	b.WriteString("{\"")
	b.WriteString(p.kind)
	b.WriteString("\", [\n")
	first := true
	for _, kid := range p.kids {
		if !first {
			b.WriteString(",\n")
		}
		first = false
		b.WriteString(kid.IndentString(1 + indent))
	}
	b.WriteString("\n")
	b.WriteString(indentStr)
	b.WriteString("]}")
	return b.String()
}

/* --- Methods of type TerminalNode --- */

func (t *TerminalNode) Kind() string {
	return t.kind
}

func (t *TerminalNode) Value() []byte {
	return t.val
}

func (t *TerminalNode) String() string {
	return t.IndentString(0)
}

func (t *TerminalNode) IndentString(indent int) string {
	indentStr := strings.Repeat("  ", indent)
	return fmt.Sprintf("%s{\"%s\", \"%s\"}", indentStr, t.kind, string(t.val))
}

func (t *TerminalNode) Kids() []node {
	return []node{}
}

