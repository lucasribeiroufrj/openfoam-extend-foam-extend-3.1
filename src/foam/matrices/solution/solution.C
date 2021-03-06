/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | foam-extend: Open Source CFD
   \\    /   O peration     |
    \\  /    A nd           | For copyright notice see file Copyright
     \\/     M anipulation  |
-------------------------------------------------------------------------------
License
    This file is part of foam-extend.

    foam-extend is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the
    Free Software Foundation, either version 3 of the License, or (at your
    option) any later version.

    foam-extend is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with foam-extend.  If not, see <http://www.gnu.org/licenses/>.

\*---------------------------------------------------------------------------*/

#include "solution.H"
#include "objectRegistry.H"

// These are for old syntax compatibility:
#include "BICCG.H"
#include "ICCG.H"
#include "IStringStream.H"

// * * * * * * * * * * * * * * Static Data Members * * * * * * * * * * * * * //

int Foam::solution::debug(::Foam::debug::debugSwitch("solution", 0));

// List of sub-dictionaries to rewrite
//! @cond localScope
static const Foam::List<Foam::word> subDictNames
(
    Foam::IStringStream("(preconditioner smoother)")()
);
//! @endcond localScope


// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::solution::solution(const objectRegistry& obr, const fileName& dictName)
:
    IOdictionary
    (
        IOobject
        (
            dictName,
            obr.time().system(),
            obr,
            IOobject::READ_IF_PRESENT,  // Allow default dictionary creation
            IOobject::NO_WRITE
        )
    ),
    relaxationFactors_
    (
        ITstream("relaxationFactors", tokenList())()
    ),
    defaultRelaxationFactor_(0),
    solvers_(ITstream("solvers", tokenList())())
{
    if (!headerOk())
    {
        if (debug)
        {
            InfoIn
            (
                "Foam::solution::solution(const objectRegistry& obr, "
                "const fileName& dictName)"
            )   << "Solution dictionary not found.  Adding default entries"
                << endl;
        }
    }

    read();
}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

Foam::label Foam::solution::upgradeSolverDict
(
    dictionary& dict,
    const bool verbose
)
{
    label nChanged = 0;

    // backward compatibility:
    // recast primitive entries into dictionary entries
    forAllIter(dictionary, dict, iter)
    {
        if (!iter().isDict())
        {
            Istream& is = iter().stream();
            word name(is);
            dictionary subdict;

            if (name == "BICCG")
            {
                // special treatment for very old syntax
                subdict = BICCG::solverDict(is);
            }
            else if (name == "ICCG")
            {
                // special treatment for very old syntax
                subdict = ICCG::solverDict(is);
            }
            else
            {
                subdict.add("solver", name);
                subdict <<= dictionary(is);

                // preconditioner and smoother entries can be
                // 1) primitiveEntry w/o settings,
                // 2) or a dictionaryEntry.
                // transform primitiveEntry with settings -> dictionaryEntry
                forAll(subDictNames, dictI)
                {
                    const word& dictName = subDictNames[dictI];
                    entry* ePtr = subdict.lookupEntryPtr(dictName,false,false);

                    if (ePtr && !ePtr->isDict())
                    {
                        Istream& is = ePtr->stream();
                        is >> name;

                        if (!is.eof())
                        {
                            dictionary newDict;
                            newDict.add(dictName, name);
                            newDict <<= dictionary(is);

                            subdict.set(dictName, newDict);
                        }
                    }
                }
            }


            // write out information to help people adjust to the new syntax
            if (verbose && Pstream::master())
            {
                Info<< "// using new solver syntax:\n"
                    << iter().keyword() << subdict << endl;
            }

            // overwrite with dictionary entry
            dict.set(iter().keyword(), subdict);

            nChanged++;
        }
    }

    return nChanged;
}


bool Foam::solution::read()
{
    bool readOk = false;

    if (headerOk())
    {
        readOk = regIOobject::read();
    }

    if (readOk)
    {
        if (found("relaxationFactors"))
        {
            relaxationFactors_ = subDict("relaxationFactors");
        }

        relaxationFactors_.readIfPresent("default", defaultRelaxationFactor_);

        if (found("solvers"))
        {
            solvers_ = subDict("solvers");
            upgradeSolverDict(solvers_);
        }

        return true;
    }

    return readOk;
}


bool Foam::solution::relax(const word& name) const
{
    if (debug)
    {
        Info<< "Find relax for " << name << endl;
    }

    return
        relaxationFactors_.found(name)
     || relaxationFactors_.found("default");
}


Foam::scalar Foam::solution::relaxationFactor(const word& name) const
{
    if (debug)
    {
        Info<< "Lookup relaxationFactor for " << name << endl;
    }

    if (relaxationFactors_.found(name))
    {
        return readScalar(relaxationFactors_.lookup(name));
    }
    else if (defaultRelaxationFactor_ > SMALL)
    {
        return defaultRelaxationFactor_;
    }
    else
    {
        FatalIOErrorIn
        (
            "Foam::solution::relaxationFactor(const word&)",
            relaxationFactors_
        )   << "Cannot find relaxationFactor for '" << name
            << "' or a suitable default value."
            << exit(FatalIOError);

        return 0;
    }
}


const Foam::dictionary& Foam::solution::solverDict(const word& name) const
{
    if (debug)
    {
        InfoIn("solution::solverDict(const word&)")
            << "Lookup solver for " << name << endl;
    }

    return solvers_.subDict(name);
}


const Foam::dictionary& Foam::solution::solver(const word& name) const
{
    if (debug)
    {
        InfoIn("solution::solver(const word&)")
            << "Lookup solver for " << name << endl;
    }

    return solvers_.subDict(name);
}


bool Foam::solution::writeData(Ostream& os) const
{
    // Write dictionaries
    os << nl << "solvers";
    solvers_.write(os, true);

    os << nl << "relaxationFactors";
    relaxationFactors_.write(os, true);

    // Write direct entries of the solution dictionary
    // HJ, 16/Feb/2010
    dictionary::write(os, false);

    return true;
}


// ************************************************************************* //
