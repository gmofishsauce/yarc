package peg

import "bufio"
import "io/ioutil"
import "path"
import "strings"

import "fmt"
import "testing"

/*

For the parser we don't want to write a zillion little test cases and embed
bits of ISACode in the test code. We want data-driven tests. So...

The tests are driven by files in the test_data directory. Each file tests a
portion of the grammar. Each test file contains data for multiple tests.

A special complication with parser testing is that in the end, only a "goal"
production, typically a complete program in the parsed language, can ever
result in a valid parse. This makes development more difficult and would mean
that all the test files would be large (i.e. complete ISAcode programs).

We avoid this by giving the parser generator (pigeon) a long list of alternate
parse rules that it may allow as a "complete" parse. When we call the parser
for real from the _isa_ command, we just check that the top of the returned
parse tree is the full program goal nonterminal and not some subgoal. Here in
the tests, we use all the subgoals.

How do know the list of parse rules (nonterminals) to allow as testable
subgoals? By enumerating the test files in the isa/peg/test_data directory.
Each test file is named for the parser subgoal it tests, e.g. SimpleSymbol,
ConstExpr, etc. There is a little build script called mk that enumerates these
files and passes the list of their names to pigeon. Later the test code here
enumerates the files and performs the tests.

In test files, empty line and lines starting with '#' are ignored. Lines
starting with '$' control the test and delimit blocks of ISACode that are the
test data to be parsed.

Example (the real ones are in test_data):

# This is an example
$mode positive
$case bin1
0b1010001

The above parses "0b1010001" seeking Number as the goal node. The test is
expected to succeed, meaning the mode is positive. The name of the test
case is bin1.
*/

const testData = "./test_data"

// Read each of the files in ./test_data and run the parser on each.
func TestGrammar(t *testing.T) {
	fileInfos, err := ioutil.ReadDir(testData)
	if err != nil {
		t.Error(err)
	}
	for _, fileInfo := range fileInfos {
		if strings.HasPrefix(fileInfo.Name(), ".") {
			// ignore vi's temp files
			continue
		}
		fileName := fileInfo.Name()
		text, err := ioutil.ReadFile(path.Join(testData, fileName))
		if err != nil {
			t.Error(err)
		}
		runTestFile(t, fileName, string(text))
	}
}

// Read and process one test data file. Interpret each control line
// to prepare for the next test. Pass non-control lines to the parser
// and report the results.
func runTestFile(t *testing.T, fileName string, text string) {
	fmt.Printf("==== %s ====\n", fileName)

	var b strings.Builder
	isPositive := false
	testName := "unset"
	parseGoal := fileName
	debug := false
	print := false
	scanner := bufio.NewScanner(strings.NewReader(text))
	lineNumber := 0

	for scanner.Scan() {
		line := strings.TrimSpace(scanner.Text())
		lineNumber += 1
		if len(line) == 0 || strings.HasPrefix(line, "#") {
			continue
		}

		// If it's not a test control line, it's part of a
		// test. Just accumulated it into the buffer.
		if !strings.HasPrefix(line, "$") {
			b.WriteString(line)
			b.WriteRune('\n')
			continue
		}

		// Found a test control line. Before processing it,
		// run the accumulated test, if any.
		if b.Len() > 0 {
			runTest(t, testName, b.String(), parseGoal, debug, isPositive, print)
			b.Reset()
			testName = "unset"
		}

		words := strings.Split(line, " ")
		if len(words) < 2 {
			t.Error(fmt.Errorf("%s:%d: bad test control line", fileName, lineNumber))
			continue
		}
		val := words[1]
		switch line[1] {
			case 'c':			// "case 
				testName = val
			case 'd':			// "debug" - voluminous debug from parser
				if val == "on" {
					debug = true
				} else {
					debug = false
				}
			case 'm':			// "mode"
				isPositive = strings.HasPrefix(val, "pos")
			case 'p':			// "print" (the parse tree after the test, if possible)
				if val == "on" {
					print = true
				} else {
					print = false
				}
			default:
				t.Error(fmt.Errorf("%s:%d: unknown test control line", fileName, lineNumber))
		}
	}
	if b.Len() > 0 {
		// Run the last test in the file	
		runTest(t, testName, b.String(), parseGoal, debug, isPositive, print)
	}
}

func getParseResultKind(parseResult interface{}) string {
	if parseResult == nil {
		return "nil"
	} else if node, ok := parseResult.(node); ok {
		return node.Kind()
	}
	return fmt.Sprintf("%v", parseResult) 
}

func runTest(t *testing.T, testName string, body string, goal string,
			 debug bool, success_expected bool, print bool) {
	testType := "negative"
	if success_expected {
		testType = "positive"
	}
	fmt.Printf("Test %s (%s test) ...", testName, testType)

	result, err := Parse(testName, []byte(body), Entrypoint(goal), Debug(debug))
	resultKind := getParseResultKind(result)

	if success_expected {
		if err == nil {
			// No error, but did the parse work?
			if resultKind != goal {
				errstr := fmt.Sprintf("failed, bad goal node %s", resultKind)
				fmt.Println(errstr)
				t.Error(fmt.Errorf(errstr))
			} else {
				fmt.Println("ok")
				if print {
					fmt.Println(result)
				}
			}
		} else {
			msg := fmt.Sprintf("%v", err)
			fmt.Printf(msg + "\n")
			t.Error(msg)
		}
	} else {
		if err != nil {
			fmt.Println("ok")
		} else {
			errstr := fmt.Sprintf("parse should have failed, but found goal node %s", resultKind)
			fmt.Println(result) // always print the parse tree in this case
			fmt.Println(errstr)
			t.Error(fmt.Errorf(errstr))
		}
	}
}
