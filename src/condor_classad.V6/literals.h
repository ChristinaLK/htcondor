/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2001, CONDOR Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI, and Rajesh Raman.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU Lesser General
 * Public License as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 * USA
 *
 *********************************************************************/

#ifndef __LITERALS_H__
#define __LITERALS_H__

BEGIN_NAMESPACE( classad )

typedef std::vector<ExprTree*> ArgumentList;

/** Represents the literals of the ClassAd language, such as integers,
		reals, booleans, strings, undefined and real.
*/
class Literal : public ExprTree 
{
  	public:
		/// Destructor
    	~Literal ();

		/** Create an absolute time literal.
		 * 	@param now The time in UNIX epoch.  If a value of NULL is passed in
		 * 	the system's current time will be used.
		 * 	@return The literal expression.
		 */
		static Literal* MakeAbsTime( abstime_t *now=NULL );

		/* Creates an absolute time literal, from the string timestr, 
		 *parsing it as the regular expression:
		 D* dddd [D* dd [D* dd [D* dd [D* dd [D* dd D*]]]]] [-dddd | +dddd | z | Z]
		 D => non-digit, d=> digit
		 Ex - 2003-01-25T09:00:00-0600
		*/
		static Literal* MakeAbsTime( std::string timestr);

		/** Create a relative time literal.
		 * @param secs The number of seconds.  If a value of -1 is passed in
		 * the time since midnight (i.e., the daytime) is used.
		 * @return The literal expression.
		 */
		static Literal* MakeRelTime( time_t secs=-1 );

		/** Create a relative time interval by subtracting two absolute times.
		 * @param t1 The end time of the interval.  If -1 is passed in, the
		 * system's current time will be used.
		 * @param t2 the start time of the interval  If -1 is passed in, the
		 * system's current time will be used.
		 * @return The literal expression of the relative time (t1 - t2).
		 */
		static Literal* MakeRelTime( time_t t1, time_t t2 );

		/* Creates a relative time literal, from the string timestr, 
		 *parsing it as [[[days+]hh:]mm:]ss
		 * Ex - 1+00:02:00
		*/		
		static Literal* MakeRelTime(std::string str);

		/* Creates a Real literal, from the string realstr,
		 * according to the ieee754 norms
		 */
		static Literal* MakeReal(std::string realstr);

		/// Make a deep copy
        // We should return an AttributeReference, but Visual
        // Studio 6 won't accept that part of the standard. 
		virtual ExprTree* Copy( ) const;

		/** Factory method to construct a Literal
		 * @param v The value to convert to a literal. (Cannot be a classad or
		 * 			list value.)
		 * @param f The number factor (B, K, M, G, T) --- ignored for
		 * 			non-numeric values.
		 * @return The constructed literal expression
		 */
		static Literal*MakeLiteral( const Value& v, Value::NumberFactor f=
					Value::NO_FACTOR );

		/** Deconstructor to get the components of the literal 
		 * 	@param v The encapsulated value
		 * 	@param f The number factor (invalid if v is non-numeric)
		 */
		void GetComponents( Value& v, Value::NumberFactor &f ) const;

		/** Deconstructor to get the encapsulated value
		 * 	@param v The value encapsulated by the literal
		 */
		void GetValue( Value& v ) const;
		
		/* Takes the number of seconds since the epoch as argument - epochsecs, 
		 *and returns the timezone offset(relative to GMT) in the currect locality
		 */
		static int findOffset(time_t epochsecs);

	protected:
		/// Constructor
    	Literal ();

  	private:
		friend class FunctionCall;
		friend class ClassAd;
		friend class ExprList;
		friend class Operation;

		virtual void _SetParentScope( const ClassAd* ){ }
		virtual bool _Flatten( EvalState&, Value&, ExprTree*&, int* ) const;
 		virtual bool _Evaluate (EvalState &, Value &) const;
 		virtual bool _Evaluate (EvalState &, Value &, ExprTree *&) const;

		// literal specific information
    	Value   			value;
		Value::NumberFactor	factor;
};

END_NAMESPACE // classad

#endif//__LITERALS_H__
